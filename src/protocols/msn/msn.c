#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include "gaim.h"
#include "prpl.h"
#include "proxy.h"
#include "md5.h"

#include "pixmaps/msn_online.xpm"
#include "pixmaps/msn_away.xpm"

#define MSN_BUF_LEN 8192
#define MIME_HEADER	"MIME-Version: 1.0\r\n" \
			"Content-Type: text/plain; charset=UTF-8\r\n" \
			"X-MMS-IM-Format: FN=MS%20Sans%20Serif; EF=; CO=0; PF=0\r\n\r\n"

#define MSN_ONLINE  1
#define MSN_BUSY    2
#define MSN_IDLE    3
#define MSN_BRB     4
#define MSN_AWAY    5
#define MSN_PHONE   6
#define MSN_LUNCH   7
#define MSN_OFFLINE 8
#define MSN_HIDDEN  9

#define USEROPT_HOTMAIL 0

struct msn_data {
	int fd;
	int trId;
	int inpa;
	GSList *switches;
	GSList *fl;
	gboolean imported;
};

struct msn_switchboard {
	struct gaim_connection *gc;
	struct conversation *chat;
	int fd;
	int inpa;
	char *sessid;
	char *auth;
	int trId;
	int total;
	char *user;
	char *txqueue;
};

struct msn_buddy {
	char *user;
	char *friend;
};

static void msn_login_callback(gpointer, gint, GaimInputCondition);
static void msn_login_xfr_connect(gpointer, gint, GaimInputCondition);

#define GET_NEXT(tmp)	while (*(tmp) && !isspace(*(tmp))) \
				(tmp)++; \
			*(tmp)++ = 0; \
			while (*(tmp) && isspace(*(tmp))) \
				(tmp)++;

static char *msn_name()
{
	return "MSN";
}

static char *msn_normalize(const char *s)
{
	static char buf[BUF_LEN];

	g_return_val_if_fail(s != NULL, NULL);

	g_snprintf(buf, sizeof(buf), "%s%s", s, strchr(s, '@') ? "" : "@hotmail.com");

	return buf;
}

static int msn_write(int fd, void *data, int len)
{
	debug_printf("C: %s", data);
	return write(fd, data, len);
}

static char *url_decode(const char *msg)
{
	static char buf[MSN_BUF_LEN];
	int i, j = 0;

	bzero(buf, sizeof(buf));
	for (i = 0; i < strlen(msg); i++) {
		char hex[3];
		if (msg[i] != '%') {
			buf[j++] = msg[i];
			continue;
		}
		strncpy(hex, msg + ++i, 2); hex[2] = 0;
		/* i is pointing to the start of the number */
		i++; /* now it's at the end and at the start of the for loop
			will be at the next character */
		buf[j++] = strtol(hex, NULL, 16);
	}
	buf[j] = 0;

	return buf;
}

static char *url_encode(const char *msg)
{
	static char buf[MSN_BUF_LEN];
	int i, j = 0;

	bzero(buf, sizeof(buf));
	for (i = 0; i < strlen(msg); i++) {
		if (isalnum(msg[i]))
			buf[j++] = msg[i];
		else {
			sprintf(buf + j, "%%%02x", (unsigned char)msg[i]);
			j += 3;
		}
	}
	buf[j] = 0;

	return buf;
}

static char *handle_errcode(char *buf, gboolean show)
{
	int errcode;
	static char msg[MSN_BUF_LEN];

	buf[4] = 0;
	errcode = atoi(buf);

	switch (errcode) {
		case 200:
			g_snprintf(msg, sizeof(msg), "Syntax Error (probably a Gaim bug)");
			break;
		case 201:
			g_snprintf(msg, sizeof(msg), "Invalid Parameter (probably a Gaim bug)");
			break;
		case 205:
			g_snprintf(msg, sizeof(msg), "Invalid User");
			break;
		case 206:
			g_snprintf(msg, sizeof(msg), "Fully Qualified Domain Name missing");
			break;
		case 207:
			g_snprintf(msg, sizeof(msg), "Already Login");
			break;
		case 208:
			g_snprintf(msg, sizeof(msg), "Invalid Username");
			break;
		case 209:
			g_snprintf(msg, sizeof(msg), "Invalid Friendly Name");
			break;
		case 210:
			g_snprintf(msg, sizeof(msg), "List Full");
			break;
		case 215:
			g_snprintf(msg, sizeof(msg), "Already there");
			break;
		case 216:
			g_snprintf(msg, sizeof(msg), "Not on list");
			break;
		case 218:
			g_snprintf(msg, sizeof(msg), "Already in the mode");
			break;
		case 219:
			g_snprintf(msg, sizeof(msg), "Already in opposite list");
			break;
		case 280:
			g_snprintf(msg, sizeof(msg), "Switchboard failed");
			break;
		case 281:
			g_snprintf(msg, sizeof(msg), "Notify Transfer failed");
			break;

		case 300:
			g_snprintf(msg, sizeof(msg), "Required fields missing");
			break;
		case 302:
			g_snprintf(msg, sizeof(msg), "Not logged in");
			break;

		case 500:
			g_snprintf(msg, sizeof(msg), "Internal server error");
			break;
		case 501:
			g_snprintf(msg, sizeof(msg), "Database server error");
			break;
		case 510:
			g_snprintf(msg, sizeof(msg), "File operation error");
			break;
		case 520:
			g_snprintf(msg, sizeof(msg), "Memory allocation error");
			break;

		case 600:
			g_snprintf(msg, sizeof(msg), "Server busy");
			break;
		case 601:
			g_snprintf(msg, sizeof(msg), "Server unavailable");
			break;
		case 602:
			g_snprintf(msg, sizeof(msg), "Peer Notification server down");
			break;
		case 603:
			g_snprintf(msg, sizeof(msg), "Database connect error");
			break;
		case 604:
			g_snprintf(msg, sizeof(msg), "Server is going down (abandon ship)");
			break;

		case 707:
			g_snprintf(msg, sizeof(msg), "Error creating connection");
			break;
		case 711:
			g_snprintf(msg, sizeof(msg), "Unable to write");
			break;
		case 712:
			g_snprintf(msg, sizeof(msg), "Session overload");
			break;
		case 713:
			g_snprintf(msg, sizeof(msg), "User is too active");
			break;
		case 714:
			g_snprintf(msg, sizeof(msg), "Too many sessions");
			break;
		case 715:
			g_snprintf(msg, sizeof(msg), "Not expected");
			break;
		case 717:
			g_snprintf(msg, sizeof(msg), "Bad friend file");
			break;

		case 911:
			g_snprintf(msg, sizeof(msg), "Authentication failed");
			break;
		case 913:
			g_snprintf(msg, sizeof(msg), "Not allowed when offline");
			break;
		case 920:
			g_snprintf(msg, sizeof(msg), "Not accepting new users");
			break;

		default:
			g_snprintf(msg, sizeof(msg), "Unknown Error Code");
			break;
	}

	if (show)
		do_error_dialog(msg, "MSN Error");

	return msg;
}

static void handle_hotmail(struct gaim_connection *gc, char *data)
{
	if (strstr(data, "Content-Type: text/x-msmsgsinitialemailnotification;")) {
		char *x = strstr(data, "Inbox-Unread:");
		if (!x) return;
		x += strlen("Inbox-Unread: ");
		connection_has_mail(gc, atoi(x), NULL, NULL);
	} else if (strstr(data, "Content-Type: text/x-msmsgsemailnotification;")) {
		char *from = strstr(data, "From:");
		char *subject = strstr(data, "Subject:");
		char *x;
		if (!from || !subject) {
			connection_has_mail(gc, 1, NULL, NULL);
			return;
		}
		from += strlen("From: ");
		x = strstr(from, "\r\n"); *x = 0;
		subject += strlen("Subject: ");
		x = strstr(subject, "\r\n"); *x = 0;
		connection_has_mail(gc, -1, from, subject);
	}
}

static struct msn_switchboard *msn_find_switch(struct gaim_connection *gc, char *id)
{
	struct msn_data *md = gc->proto_data;
	GSList *m = md->switches;

	while (m) {
		struct msn_switchboard *ms = m->data;
		m = m->next;
		if ((ms->total == 1) && !g_strcasecmp(ms->user, id))
			return ms;
	}

	return NULL;
}

static struct msn_switchboard *msn_find_switch_by_id(struct gaim_connection *gc, int id)
{
	struct msn_data *md = gc->proto_data;
	GSList *m = md->switches;

	while (m) {
		struct msn_switchboard *ms = m->data;
		m = m->next;
		if (ms->chat && (ms->chat->id == id))
			return ms;
	}

	return NULL;
}

static struct msn_switchboard *msn_find_writable_switch(struct gaim_connection *gc)
{
	struct msn_data *md = gc->proto_data;
	GSList *m = md->switches;

	while (m) {
		struct msn_switchboard *ms = m->data;
		m = m->next;
		if (ms->txqueue)
			return ms;
	}

	return NULL;
}

static void msn_kill_switch(struct msn_switchboard *ms)
{
	struct gaim_connection *gc = ms->gc;
	struct msn_data *md = gc->proto_data;

	if (ms->inpa)
		gaim_input_remove(ms->inpa);
	close(ms->fd);
	if (ms->sessid)
		g_free(ms->sessid);
	g_free(ms->auth);
	if (ms->user)
		g_free(ms->user);
	if (ms->txqueue)
		g_free(ms->txqueue);
	if (ms->chat)
		serv_got_chat_left(gc, ms->chat->id);

	md->switches = g_slist_remove(md->switches, ms);

	g_free(ms);
}

static void msn_switchboard_callback(gpointer data, gint source, GaimInputCondition cond)
{
	struct msn_switchboard *ms = data;
	struct gaim_connection *gc = ms->gc;
	char buf[MSN_BUF_LEN];
	static int id = 0;
	int i = 0;

	bzero(buf, sizeof(buf));
	while ((read(ms->fd, buf + i, 1) > 0) && (buf[i++] != '\n'))
		if (i == sizeof(buf))
			i--; /* yes i know this loses data but we shouldn't get messages this long
				and it's better than possibly writing past our buffer */
	if (i == 0 || buf[i - 1] != '\n') {
		msn_kill_switch(ms);
		return;
	}
	debug_printf("S: %s", buf);
	g_strchomp(buf);

	if (!g_strncasecmp(buf, "ACK", 3)) {
	} else if (!g_strncasecmp(buf, "ANS", 3)) {
		if (ms->chat)
			add_chat_buddy(ms->chat, gc->username);
	} else if (!g_strncasecmp(buf, "BYE", 3)) {
		if (ms->chat) {
			char *user, *tmp = buf;
			GET_NEXT(tmp);
			user = tmp;
			remove_chat_buddy(ms->chat, user);
		} else
			msn_kill_switch(ms);
	} else if (!g_strncasecmp(buf, "CAL", 3)) {
	} else if (!g_strncasecmp(buf, "IRO", 3)) {
		char *tot, *user, *tmp = buf;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		GET_NEXT(tmp);
		tot = tmp;
		GET_NEXT(tmp);
		ms->total = atoi(tot);
		user = tmp;
		GET_NEXT(tmp);

		if (ms->total > 1) {
			if (!ms->chat)
				ms->chat = serv_got_joined_chat(gc, ++id, "MSN Chat");
			add_chat_buddy(ms->chat, user);
		} 
	} else if (!g_strncasecmp(buf, "JOI", 3)) {
		char *user, *tmp = buf;
		GET_NEXT(tmp);
		user = tmp;
		GET_NEXT(tmp);

		if (ms->total == 1) {
			ms->chat = serv_got_joined_chat(gc, ++id, "MSN Chat");
			add_chat_buddy(ms->chat, ms->user);
			add_chat_buddy(ms->chat, gc->username);
			g_free(ms->user);
			ms->user = NULL;
		}
		if (ms->chat)
			add_chat_buddy(ms->chat, user);
		ms->total++;
		if (ms->txqueue) {
			char *utf8 = str_to_utf8(ms->txqueue);
			g_snprintf(buf, sizeof(buf), "MSG %d N %d\r\n%s%s", ++ms->trId,
					strlen(MIME_HEADER) + strlen(utf8),
					MIME_HEADER, utf8);
			g_free(utf8);
			g_free(ms->txqueue);
			ms->txqueue = NULL;
			if (msn_write(ms->fd, buf, strlen(buf)) < 0)
				msn_kill_switch(ms);
			debug_printf("\n");
		}
	} else if (!g_strncasecmp(buf, "MSG", 3)) {
		char *user, *tmp = buf;
		int length;
		char *msg, *content, *utf;
		int len, r;

		GET_NEXT(tmp);
		user = tmp;

		GET_NEXT(tmp);

		GET_NEXT(tmp);
		length = atoi(tmp);

		msg = g_new0(char, MAX(length + 1, MSN_BUF_LEN));

		for (len = 0; len < length; len += r) {
			if ((r = read(ms->fd, msg+len, length-len)) <= 0) {
				g_free(msg);
				hide_login_progress(gc, "Unable to read message");
				signoff(gc);
				return;
			}
		}

		content = strstr(msg, "Content-Type: ");
		if (!content) {
			g_free(msg);
			return;
		}
		if (!g_strncasecmp(content, "Content-Type: text/plain",
				     strlen("Content-Type: text/plain"))) {
			char *final, *skiphead;
			skiphead = strstr(msg, "\r\n\r\n");
			if (!skiphead || !skiphead[4]) {
				g_free(msg);
				return;
			}
			skiphead += 4;
			utf = utf8_to_str(skiphead);
			len = MAX(strlen(utf) + 1, BUF_LEN);
			final = g_malloc(len);
			g_snprintf(final, len, "%s", utf);
			g_free(utf);

			if (ms->chat)
				serv_got_chat_in(gc, ms->chat->id, user, 0, final, time(NULL));
			else
				serv_got_im(gc, user, final, 0, time(NULL));

			g_free(final);
		}
		g_free(msg);
	} else if (!g_strncasecmp(buf, "NAK", 3)) {
		do_error_dialog("A message may not have been received.", "MSN Error");
	} else if (!g_strncasecmp(buf, "NLN", 3)) {
	} else if (!g_strncasecmp(buf, "OUT", 3)) {
		if (ms->chat)
			serv_got_chat_left(gc, ms->chat->id);
		msn_kill_switch(ms);
	} else if (!g_strncasecmp(buf, "USR", 3)) {
		/* good, we got USR, now we need to find out who we want to talk to */
		struct msn_switchboard *ms = msn_find_writable_switch(gc);

		if (!ms)
			return;

		g_snprintf(buf, sizeof(buf), "CAL %d %s\n", ++ms->trId, ms->user);
		if (msn_write(ms->fd, buf, strlen(buf)) < 0)
			msn_kill_switch(ms);
	} else if (isdigit(*buf)) {
		handle_errcode(buf, TRUE);
	} else {
		debug_printf("Unhandled message!\n");
	}
}

static void msn_rng_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct msn_switchboard *ms = data;
	struct gaim_connection *gc = ms->gc;
	struct msn_data *md;
	char buf[MSN_BUF_LEN];

	if (source == -1 || !g_slist_find(connections, gc)) {
		close(source);
		g_free(ms->sessid);
		g_free(ms->auth);
		g_free(ms);
		return;
	}

	md = gc->proto_data;

	if (ms->fd != source)
		ms->fd = source;

	g_snprintf(buf, sizeof(buf), "ANS %d %s %s %s\n", ++ms->trId, gc->username, ms->auth, ms->sessid);
	if (msn_write(ms->fd, buf, strlen(buf)) < 0) {
		close(ms->fd);
		g_free(ms->sessid);
		g_free(ms->auth);
		g_free(ms);
		return;
	}

	md->switches = g_slist_append(md->switches, ms);
	ms->inpa = gaim_input_add(ms->fd, GAIM_INPUT_READ, msn_switchboard_callback, ms);
}

static void msn_ss_xfr_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct msn_switchboard *ms = data;
	struct gaim_connection *gc = ms->gc;
	char buf[MSN_BUF_LEN];

	if (source == -1 || !g_slist_find(connections, gc)) {
		close(source);
		g_free(ms->auth);
		g_free(ms);
		return;
	}

	if (ms->fd != source)
		ms->fd = source;

	g_snprintf(buf, sizeof(buf), "USR %d %s %s\n", ++ms->trId, gc->username, ms->auth);
	if (msn_write(ms->fd, buf, strlen(buf)) < 0) {
		g_free(ms->auth);
		g_free(ms);
		return;
	}

	ms->inpa = gaim_input_add(ms->fd, GAIM_INPUT_READ, msn_switchboard_callback, ms);
}

struct msn_add_permit {
	struct gaim_connection *gc;
	char *user;
	char *friend;
};

static void msn_accept_add(gpointer w, struct msn_add_permit *map)
{
	struct msn_data *md = map->gc->proto_data;
	char buf[MSN_BUF_LEN];

	g_snprintf(buf, sizeof(buf), "ADD %d AL %s %s\n", ++md->trId, map->user, map->friend);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(map->gc, "Write error");
		signoff(map->gc);
		return;
	}
}

static void msn_cancel_add(gpointer w, struct msn_add_permit *map)
{
	g_free(map->user);
	g_free(map->friend);
	g_free(map);
}

static void msn_callback(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];
	int i = 0;

	bzero(buf, sizeof(buf));
	while ((read(md->fd, buf + i, 1) > 0) && (buf[i++] != '\n'))
		if (i == sizeof(buf))
			i--; /* yes i know this loses data but we shouldn't get messages this long
				and it's better than possibly writing past our buffer */
	if (i == 0 || buf[i - 1] != '\n') {
		hide_login_progress(gc, "Error reading from server");
		signoff(gc);
		return;
	}
	debug_printf("S: %s", buf);
	g_strchomp(buf);

	if (!g_strncasecmp(buf, "ADD", 3)) {
		char *list, *user, *friend, *tmp = buf;
		struct msn_add_permit *ap = g_new0(struct msn_add_permit, 1);
		char msg[MSN_BUF_LEN];

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		list = tmp;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		user = tmp;

		GET_NEXT(tmp);
		friend = tmp;

		if (g_strcasecmp(list, "RL"))
			return;

		ap->user = g_strdup(user);
		ap->friend = g_strdup(friend);
		ap->gc = gc;

		g_snprintf(msg, sizeof(msg), "The user %s (%s) wants to add you to their buddy list.",
				ap->user, url_decode(ap->friend));

		do_ask_dialog(msg, ap, msn_accept_add, msn_cancel_add);
	} else if (!g_strncasecmp(buf, "BLP", 3)) {
	} else if (!g_strncasecmp(buf, "BPR", 3)) {
	} else if (!g_strncasecmp(buf, "CHG", 3)) {
	} else if (!g_strncasecmp(buf, "CHL", 3)) {
		char *hash = buf;
		char buf2[MSN_BUF_LEN];
		md5_state_t st;
		md5_byte_t di[16];
		int i;

		GET_NEXT(hash);
		GET_NEXT(hash);

		md5_init(&st);
		md5_append(&st, (const md5_byte_t *)hash, strlen(hash));
		md5_append(&st, (const md5_byte_t *)"Q1P7W2E4J9R8U3S5", strlen("Q1P7W2E4J9R8U3S5"));
		md5_finish(&st, di);

		g_snprintf(buf, sizeof(buf), "QRY %d msmsgs@msnmsgr.com 32\r\n", ++md->trId);
		for (i = 0; i < 16; i++) {
			g_snprintf(buf2, sizeof(buf2), "%02x", di[i]);
			strcat(buf, buf2);
		}

		if (msn_write(md->fd, buf, strlen(buf)) < 0) {
			hide_login_progress(gc, "Unable to write to server");
			signoff(gc);
		}

		debug_printf("\n");
	} else if (!g_strncasecmp(buf, "FLN", 3)) {
		char *usr = buf;

		GET_NEXT(usr);
		serv_got_update(gc, usr, 0, 0, 0, 0, 0, 0);
	} else if (!g_strncasecmp(buf, "GTC", 3)) {
	} else if (!g_strncasecmp(buf, "INF", 3)) {
	} else if (!g_strncasecmp(buf, "ILN", 3)) {
		char *state, *user, *tmp = buf;
		int status = UC_NORMAL;

		GET_NEXT(tmp);

		GET_NEXT(tmp);
		state = tmp;

		GET_NEXT(tmp);
		user = tmp;

		GET_NEXT(tmp);

		if (!g_strcasecmp(state, "BSY")) {
			status |= (MSN_BUSY << 5);
		} else if (!g_strcasecmp(state, "IDL")) {
			status |= (MSN_IDLE << 5);
		} else if (!g_strcasecmp(state, "BRB")) {
			status |= (MSN_BRB << 5);
		} else if (!g_strcasecmp(state, "AWY")) {
			status = UC_UNAVAILABLE;
		} else if (!g_strcasecmp(state, "PHN")) {
			status |= (MSN_PHONE << 5);
		} else if (!g_strcasecmp(state, "LUN")) {
			status |= (MSN_LUNCH << 5);
		}

		serv_got_update(gc, user, 1, 0, 0, 0, status, 0);
	} else if (!g_strncasecmp(buf, "LST", 3)) {
		char *which, *who, *friend, *tmp = buf;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		which = tmp;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		GET_NEXT(tmp);
		GET_NEXT(tmp);
		who = tmp;

		GET_NEXT(tmp);
		friend = url_decode(tmp);

		if (!g_strcasecmp(which, "FL")) {
			struct msn_buddy *b = g_new0(struct msn_buddy, 1);
			b->user = g_strdup(who);
			b->friend = g_strdup(friend);
			md->fl = g_slist_append(md->fl, b);
		} else if (!md->imported) {
			if (bud_list_cache_exists(gc))
				do_import(NULL, gc);
			md->imported = TRUE;
			while (md->fl) {
				struct msn_buddy *mb = md->fl->data;
				struct buddy *b;
				md->fl = g_slist_remove(md->fl, mb);
				if (!(b = find_buddy(gc, mb->user)))
					add_buddy(gc, "Buddies", mb->user, mb->friend);
				else if (!g_strcasecmp(b->name, b->show)) {
					g_snprintf(b->show, sizeof(b->show), "%s", mb->friend);
					handle_buddy_rename(b, b->name);
				}
				g_free(mb->user);
				g_free(mb->friend);
				g_free(mb);
			}
		}
	} else if (!g_strncasecmp(buf, "MSG", 3)) {
		char *user, *tmp = buf;
		int length;
		char *msg, *skiphead, *utf, *final;
		int len, r;

		GET_NEXT(tmp);
		user = tmp;

		GET_NEXT(tmp);

		GET_NEXT(tmp);
		length = atoi(tmp);

		msg = g_new0(char, MAX(length + 1, MSN_BUF_LEN));

		for (len = 0; len < length; len += r) {
			if ((r = read(md->fd, msg+len, length-len)) <= 0) {
				g_free(msg);
				hide_login_progress(gc, "Unable to read message");
				signoff(gc);
				return;
			}
		}

		if (!g_strcasecmp(user, "hotmail")) {
			handle_hotmail(gc, msg);
			g_free(msg);
			return;
		}

		skiphead = strstr(msg, "\r\n\r\n");
		if (!skiphead || !skiphead[4]) {
			g_free(msg);
			return;
		}
		skiphead += 4;
		utf = utf8_to_str(skiphead);
		len = MAX(strlen(utf) + 1, BUF_LEN);
		final = g_malloc(len);
		g_snprintf(final, len, "%s", utf);
		g_free(utf);

		serv_got_im(gc, user, final, 0, time(NULL));

		g_free(final);
		g_free(msg);
	} else if (!g_strncasecmp(buf, "NLN", 3)) {
		char *state, *user, *tmp = buf;
		int status = UC_NORMAL;

		GET_NEXT(tmp);
		state = tmp;

		GET_NEXT(tmp);
		user = tmp;

		GET_NEXT(tmp);

		if (!g_strcasecmp(state, "BSY")) {
			status |= (MSN_BUSY << 5);
		} else if (!g_strcasecmp(state, "IDL")) {
			status |= (MSN_IDLE << 5);
		} else if (!g_strcasecmp(state, "BRB")) {
			status |= (MSN_BRB << 5);
		} else if (!g_strcasecmp(state, "AWY")) {
			status = UC_UNAVAILABLE;
		} else if (!g_strcasecmp(state, "PHN")) {
			status |= (MSN_PHONE << 5);
		} else if (!g_strcasecmp(state, "LUN")) {
			status |= (MSN_LUNCH << 5);
		}

		serv_got_update(gc, user, 1, 0, 0, 0, status, 0);
	} else if (!g_strncasecmp(buf, "OUT", 3)) {
	} else if (!g_strncasecmp(buf, "PRP", 3)) {
	} else if (!g_strncasecmp(buf, "QRY", 3)) {
	} else if (!g_strncasecmp(buf, "REA", 3)) {
		char *friend, *tmp = buf;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		GET_NEXT(tmp);
		GET_NEXT(tmp);
		friend = url_decode(tmp);

		g_snprintf(gc->displayname, sizeof(gc->displayname), "%s", friend);
	} else if (!g_strncasecmp(buf, "REM", 3)) {
	} else if (!g_strncasecmp(buf, "RNG", 3)) {
		struct msn_switchboard *ms;
		char *sessid, *ssaddr, *auth, *user;
		int port, i = 0;
		char *tmp = buf;

		GET_NEXT(tmp);
		sessid = tmp;

		GET_NEXT(tmp);
		ssaddr = tmp;

		GET_NEXT(tmp);

		GET_NEXT(tmp);
		auth = tmp;

		GET_NEXT(tmp);
		user = tmp;
		GET_NEXT(tmp);

		while (ssaddr[i] && ssaddr[i] != ':') i++;
		if (ssaddr[i] == ':') {
			char *x = &ssaddr[i + 1];
			ssaddr[i] = 0;
			port = atoi(x);
		} else
			port = 1863;

		ms = g_new0(struct msn_switchboard, 1);
		ms->user = g_strdup(user);
		ms->sessid = g_strdup(sessid);
		ms->auth = g_strdup(auth);
		ms->gc = gc;
		ms->fd = proxy_connect(ssaddr, port, msn_rng_connect, ms);
	} else if (!g_strncasecmp(buf, "SYN", 3)) {
	} else if (!g_strncasecmp(buf, "USR", 3)) {
	} else if (!g_strncasecmp(buf, "XFR", 3)) {
		char *host = strstr(buf, "SB");
		int port;
		int i = 0;
		gboolean switchboard = TRUE;
		char *tmp;

		if (!host) {
			host = strstr(buf, "NS");
			if (!host) {
				hide_login_progress(gc, "Got invalid XFR\n");
				signoff(gc);
				return;
			}
			switchboard = FALSE;
		}

		GET_NEXT(host);
		while (host[i] && host[i] != ':') i++;
		if (host[i] == ':') {
			tmp = &host[i + 1];
			host[i] = 0;
			while (isdigit(*tmp)) tmp++;
			*tmp++ = 0;
			port = atoi(&host[i + 1]);
		} else {
			port = 1863;
			tmp = host;
			GET_NEXT(tmp);
		}

		if (switchboard) {
			struct msn_switchboard *ms = msn_find_writable_switch(gc);
			if (!ms)
				return;

			GET_NEXT(tmp);

			ms->auth = g_strdup(tmp);
			ms->fd = proxy_connect(host, port, msn_ss_xfr_connect, ms);
		} else {
			close(md->fd);
			gaim_input_remove(md->inpa);
			md->inpa = 0;
			md->fd = 0;
			md->fd = proxy_connect(host, port, msn_login_xfr_connect, gc);
		}
	} else if (isdigit(*buf)) {
		handle_errcode(buf, TRUE);
	} else {
		debug_printf("Unhandled message!\n");
	}
}

static void msn_login_xfr_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct msn_data *md;
	char buf[MSN_BUF_LEN];

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	md = gc->proto_data;

	if (md->fd != source)
		md->fd = source;

	if (md->fd == -1) {
		hide_login_progress(gc, "Unable to connect to Notification Server");
		signoff(gc);
		return;
	}

	g_snprintf(buf, sizeof(buf), "VER %d MSNP5\n", ++md->trId);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, "Unable to talk to Notification Server");
		signoff(gc);
		return;
	}

	md->inpa = gaim_input_add(md->fd, GAIM_INPUT_READ, msn_login_callback, gc);
}

static void msn_login_callback(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];
	int i = 0;

	bzero(buf, sizeof(buf));
	while ((read(md->fd, buf + i, 1) > 0) && (buf[i++] != '\n'))
		if (i == sizeof(buf))
			i--; /* yes i know this loses data but we shouldn't get messages this long
				and it's better than possibly writing past our buffer */
	if (i == 0 || buf[i - 1] != '\n') {
		hide_login_progress(gc, "Error reading from server");
		signoff(gc);
		return;
	}
	debug_printf("S: %s", buf);
	g_strchomp(buf);

	if (!g_strncasecmp(buf, "VER", 3)) {
		/* we got VER, check to see that MSNP5 is in the list, then send INF */
		if (!strstr(buf, "MSNP5")) {
			hide_login_progress(gc, "Protocol not supported");
			signoff(gc);
			return;
		}

		g_snprintf(buf, sizeof(buf), "INF %d\n", ++md->trId);
		if (msn_write(md->fd, buf, strlen(buf)) < 0) {
			hide_login_progress(gc, "Unable to request INF\n");
			signoff(gc);
			return;
		}
	} else if (!g_strncasecmp(buf, "INF", 3)) {
		/* check to make sure we can use md5 */
		if (!strstr(buf, "MD5")) {
			hide_login_progress(gc, "Unable to login using MD5");
			signoff(gc);
			return;
		}

		g_snprintf(buf, sizeof(buf), "USR %d MD5 I %s\n", ++md->trId, gc->username);
		if (msn_write(md->fd, buf, strlen(buf)) < 0) {
			hide_login_progress(gc, "Unable to send USR\n");
			signoff(gc);
			return;
		}

		set_login_progress(gc, 3, "Requesting to send password");
	} else if (!g_strncasecmp(buf, "USR", 3)) {
		char *resp, *friend, *tmp = buf;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		resp = tmp;
		GET_NEXT(tmp);
		GET_NEXT(tmp);
		friend = tmp;
		GET_NEXT(tmp);
		friend = url_decode(friend);

		/* so here, we're either getting the challenge or the OK */
		if (!g_strcasecmp(resp, "OK")) {
			g_snprintf(gc->displayname, sizeof(gc->displayname), "%s", friend);

			g_snprintf(buf, sizeof(buf), "SYN %d 0\n", ++md->trId);
			if (msn_write(md->fd, buf, strlen(buf)) < 0) {
				hide_login_progress(gc, "Unable to write");
				signoff(gc);
				return;
			}

			g_snprintf(buf, sizeof(buf), "CHG %d NLN\n", ++md->trId);
			if (msn_write(md->fd, buf, strlen(buf)) < 0) {
				hide_login_progress(gc, "Unable to write");
				signoff(gc);
				return;
			}

			g_snprintf(buf, sizeof(buf), "BLP %d AL\n", ++md->trId);
			if (msn_write(md->fd, buf, strlen(buf)) < 0) {
				hide_login_progress(gc, "Unable to write");
				signoff(gc);
				return;
			}

			account_online(gc);
			serv_finish_login(gc);

			gaim_input_remove(md->inpa);
			md->inpa = gaim_input_add(md->fd, GAIM_INPUT_READ, msn_callback, gc);
		} else if (!g_strcasecmp(resp, "MD5")) {
			char buf2[MSN_BUF_LEN];
			md5_state_t st;
			md5_byte_t di[16];
			int i;

			g_snprintf(buf2, sizeof(buf2), "%s%s", friend, gc->password);

			md5_init(&st);
			md5_append(&st, (const md5_byte_t *)buf2, strlen(buf2));
			md5_finish(&st, di);

			g_snprintf(buf, sizeof(buf), "USR %d MD5 S ", ++md->trId);
			for (i = 0; i < 16; i++) {
				g_snprintf(buf2, sizeof(buf2), "%02x", di[i]);
				strcat(buf, buf2);
			}
			strcat(buf, "\n");

			if (msn_write(md->fd, buf, strlen(buf)) < 0) {
				hide_login_progress(gc, "Unable to send password");
				signoff(gc);
				return;
			}

			set_login_progress(gc, 4, "Password sent");
		}
	} else if (!g_strncasecmp(buf, "XFR", 3)) {
		char *host = strstr(buf, "NS");
		int port;
		int i = 0;

		if (!host) {
			hide_login_progress(gc, "Got invalid XFR\n");
			signoff(gc);
			return;
		}

		GET_NEXT(host);
		while (host[i] && host[i] != ':') i++;
		if (host[i] == ':') {
			char *x = &host[i + 1];
			host[i] = 0;
			port = atoi(x);
		} else
			port = 1863;

		close(md->fd);
		gaim_input_remove(md->inpa);
		md->inpa = 0;
		md->fd = 0;
		md->fd = proxy_connect(host, port, msn_login_xfr_connect, gc);
	} else {
		if (isdigit(*buf))
			hide_login_progress(gc, handle_errcode(buf, FALSE));
		else
			hide_login_progress(gc, "Unable to parse message");
		signoff(gc);
		return;
	}
}

static void msn_login_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct msn_data *md;
	char buf[1024];

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	md = gc->proto_data;

	if (md->fd != source)
		md->fd = source;

	if (md->fd == -1) {
		hide_login_progress(gc, "Unable to connect");
		signoff(gc);
		return;
	}

	g_snprintf(buf, sizeof(buf), "VER %d MSNP5\n", ++md->trId);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, "Unable to write to server");
		signoff(gc);
		return;
	}

	md->inpa = gaim_input_add(md->fd, GAIM_INPUT_READ, msn_login_callback, gc);
	set_login_progress(gc, 2, "Synching with server");
}

static void msn_login(struct aim_user *user)
{
	struct gaim_connection *gc = new_gaim_conn(user);
	struct msn_data *md = gc->proto_data = g_new0(struct msn_data, 1);

	set_login_progress(gc, 1, "Connecting");

	g_snprintf(gc->username, sizeof(gc->username), "%s", msn_normalize(gc->username));

	md->fd = proxy_connect("messenger.hotmail.com", 1863, msn_login_connect, gc);
}

static void msn_close(struct gaim_connection *gc)
{
	struct msn_data *md = gc->proto_data;
	close(md->fd);
	if (md->inpa)
		gaim_input_remove(md->inpa);
	while (md->switches)
		msn_kill_switch(md->switches->data);
	while (md->fl) {
		struct msn_buddy *tmp = md->fl->data;
		md->fl = g_slist_remove(md->fl, tmp);
		g_free(tmp->user);
		g_free(tmp->friend);
		g_free(tmp);
	}
	g_free(md);
}

static int msn_send_im(struct gaim_connection *gc, char *who, char *message, int away)
{
	struct msn_data *md = gc->proto_data;
	struct msn_switchboard *ms = msn_find_switch(gc, who);
	char buf[MSN_BUF_LEN];

	if (ms) {
		char *utf8 = str_to_utf8(message);
		g_snprintf(buf, sizeof(buf), "MSG %d N %d\r\n%s%s", ++ms->trId,
				strlen(MIME_HEADER) + strlen(utf8),
				MIME_HEADER, utf8);
		g_free(utf8);
		if (msn_write(ms->fd, buf, strlen(buf)) < 0)
			msn_kill_switch(ms);
		debug_printf("\n");
	} else if (strcmp(who, gc->username)) {
		g_snprintf(buf, MSN_BUF_LEN, "XFR %d SB\n", ++md->trId);
		if (msn_write(md->fd, buf, strlen(buf)) < 0) {
			hide_login_progress(gc, "Write error");
			signoff(gc);
			return 0;
		}

		ms = g_new0(struct msn_switchboard, 1);
		md->switches = g_slist_append(md->switches, ms);
		ms->user = g_strdup(who);
		ms->txqueue = g_strdup(message);
		ms->gc = gc;
		ms->fd = -1;
	} else
		/* in msn you can't send messages to yourself, so we'll fake like we received it ;) */
		serv_got_im(gc, who, message, away, time(NULL));
	return 0;
}

static int msn_chat_send(struct gaim_connection *gc, int id, char *message)
{
	struct msn_switchboard *ms = msn_find_switch_by_id(gc, id);
	char buf[MSN_BUF_LEN];

	if (!ms)
		return -EINVAL;

	g_snprintf(buf, sizeof(buf), "MSG %d N %d\r\n%s%s", ++ms->trId,
			strlen(MIME_HEADER) + strlen(message),
			MIME_HEADER, message);
	if (msn_write(ms->fd, buf, strlen(buf)) < 0)
		msn_kill_switch(ms);
	debug_printf("\n");
	serv_got_chat_in(gc, id, gc->username, 0, message, time(NULL));
	return 0;
}

static void msn_chat_invite(struct gaim_connection *gc, int id, char *msg, char *who)
{
	struct msn_switchboard *ms = msn_find_switch_by_id(gc, id);
	char buf[MSN_BUF_LEN];

	if (!ms)
		return;

	g_snprintf(buf, sizeof(buf), "CAL %d %s\n", ++ms->trId, who);
	if (msn_write(ms->fd, buf, strlen(buf)) < 0)
		msn_kill_switch(ms);
}

static void msn_chat_leave(struct gaim_connection *gc, int id)
{
	struct msn_switchboard *ms = msn_find_switch_by_id(gc, id);
	char buf[MSN_BUF_LEN];

	if (!ms)
		return;

	g_snprintf(buf, sizeof(buf), "OUT\n");
	if (msn_write(ms->fd, buf, strlen(buf)) < 0)
		msn_kill_switch(ms);
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
	m = g_list_append(m, "Hidden");

	return m;
}

static void msn_set_away(struct gaim_connection *gc, char *state, char *msg)
{
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];
	char *away;

	gc->away = NULL;

	if (msg) {
		gc->away = "";
		away = "AWY";
	} else if (state) {
		gc->away = "";

		if (!strcmp(state, "Away From Computer"))
			away = "AWY";
		else if (!strcmp(state, "Be Right Back"))
			away = "BRB";
		else if (!strcmp(state, "Busy"))
			away = "BSY";
		else if (!strcmp(state, "On The Phone"))
			away = "PHN";
		else if (!strcmp(state, "Out To Lunch"))
			away = "LUN";
		else if (!strcmp(state, "Hidden"))
			away = "HDN";
		else {
			gc->away = NULL;
			away = "NLN";
		}
	} else if (gc->is_idle)
		away = "IDL";
	else
		away = "NLN";

	g_snprintf(buf, sizeof(buf), "CHG %d %s\n", ++md->trId, away);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, "Write error");
		signoff(gc);
		return;
	}
}

static void msn_set_idle(struct gaim_connection *gc, int idle)
{
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];

	if (gc->away)
		return;
	if (idle)
		g_snprintf(buf, sizeof(buf), "CHG %d IDL\n", ++md->trId);
	else
		g_snprintf(buf, sizeof(buf), "CHG %d NLN\n", ++md->trId);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, "Write error");
		signoff(gc);
		return;
	}
}

static char **msn_list_icon(int uc)
{
	if (uc == UC_NORMAL)
		return msn_online_xpm;

	return msn_away_xpm;
}

static char *msn_get_away_text(int s)
{
	switch (s) {
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
			return "Available";
	}
}

static GList *msn_buddy_menu(struct gaim_connection *gc, char *who)
{
	GList *m = NULL;
	struct proto_buddy_menu *pbm;
	struct buddy *b = find_buddy(gc, who);
	static char buf[MSN_BUF_LEN];

	if (!b || !(b->uc >> 5))
		return m;

	pbm = g_new0(struct proto_buddy_menu, 1);
	g_snprintf(buf, sizeof(buf), "Status: %s", msn_get_away_text(b->uc >> 5));
	pbm->label = buf;
	pbm->callback = NULL;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	return m;
}

static void msn_add_buddy(struct gaim_connection *gc, char *who)
{
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];
	GSList *l = md->fl;

	while (l) {
		struct msn_buddy *b = l->data;
		if (!g_strcasecmp(who, b->user))
			break;
		l = l->next;
	}
	if (l)
		return;

	g_snprintf(buf, sizeof(buf), "ADD %d FL %s %s\n", ++md->trId, who, who);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, "Write error");
		signoff(gc);
		return;
	}
}

static void msn_rem_buddy(struct gaim_connection *gc, char *who)
{
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];

	g_snprintf(buf, sizeof(buf), "REM %d FL %s\n", ++md->trId, who);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, "Write error");
		signoff(gc);
		return;
	}
}

static void msn_act_id(gpointer data, char *entry)
{
	struct gaim_connection *gc = data;
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];

	g_snprintf(buf, sizeof(buf), "REA %d %s %s\n", ++md->trId, gc->username, url_encode(entry));
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, "Write error");
		signoff(gc);
		return;
	}
}

static void msn_do_action(struct gaim_connection *gc, char *act)
{
	if (!strcmp(act, "Set Friendly Name")) {
		do_prompt_dialog("Set Friendly Name:", gc, msn_act_id, NULL);
	}
}

static GList *msn_actions()
{
	GList *m = NULL;

	m = g_list_append(m, "Set Friendly Name");

	return m;
}

static struct prpl *my_protocol = NULL;

void msn_init(struct prpl *ret)
{
	ret->protocol = PROTO_MSN;
	ret->options = OPT_PROTO_MAIL_CHECK;
	ret->name = msn_name;
	ret->list_icon = msn_list_icon;
	ret->buddy_menu = msn_buddy_menu;
	ret->login = msn_login;
	ret->close = msn_close;
	ret->send_im = msn_send_im;
	ret->away_states = msn_away_states;
	ret->set_away = msn_set_away;
	ret->set_idle = msn_set_idle;
	ret->add_buddy = msn_add_buddy;
	ret->remove_buddy = msn_rem_buddy;
	ret->chat_send = msn_chat_send;
	ret->chat_invite = msn_chat_invite;
	ret->chat_leave = msn_chat_leave;
	ret->normalize = msn_normalize;
	ret->do_action = msn_do_action;
	ret->actions = msn_actions;

	my_protocol = ret;
}

#ifndef STATIC

char *gaim_plugin_init(GModule *handle)
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

char *name()
{
	return "MSN";
}

char *description()
{
	return PRPL_DESC("MSN");
}

#endif
