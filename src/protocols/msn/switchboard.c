/**
 * @file switchboard.c MSN switchboard functions
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
#include "msn.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

G_MODULE_IMPORT GSList *connections;

static char *
msn_parse_format(char *mime)
{
	char *cur;
	GString *ret = g_string_new(NULL);
	guint colorbuf;
	char *colors = (char *)(&colorbuf);
	

	cur = strstr(mime, "FN=");
	if (cur && (*(cur = cur + 3) != ';')) {
		ret = g_string_append(ret, "<FONT FACE=\"");
		while (*cur && *cur != ';') {
			ret = g_string_append_c(ret, *cur);
			cur++;
		}
		ret = g_string_append(ret, "\">");
	}
	
	cur = strstr(mime, "EF=");
	if (cur && (*(cur = cur + 3) != ';')) {
		while (*cur && *cur != ';') {
			ret = g_string_append_c(ret, '<');
			ret = g_string_append_c(ret, *cur);
			ret = g_string_append_c(ret, '>');
			cur++;
		}
	}
	
	cur = strstr(mime, "CO=");
	if (cur && (*(cur = cur + 3) != ';')) {
		if (sscanf (cur, "%x;", &colorbuf) == 1) {
			char tag[64];
			g_snprintf(tag, sizeof(tag), "<FONT COLOR=\"#%02hhx%02hhx%02hhx\">", colors[0], colors[1], colors[2]);
			ret = g_string_append(ret, tag);
		}
	}
	
	cur = url_decode(ret->str);
	g_string_free(ret, TRUE);
	return cur;
}

static int
msn_process_switch(struct msn_switchboard *ms, char *buf)
{
	struct gaim_connection *gc = ms->gc;
	char sendbuf[MSN_BUF_LEN];
	static int id = 0;

	if (!g_strncasecmp(buf, "ACK", 3)) {
	} else if (!g_strncasecmp(buf, "ANS", 3)) {
		if (ms->chat)
			gaim_chat_add_user(GAIM_CHAT(ms->chat), gc->username, NULL);
	} else if (!g_strncasecmp(buf, "BYE", 3)) {
		char *user, *tmp = buf;
		GET_NEXT(tmp);
		user = tmp;

		if (ms->chat) {
			gaim_chat_remove_user(GAIM_CHAT(ms->chat), user, NULL);
		} else {
			char msgbuf[256];
			const char *username;
			struct gaim_conversation *cnv;
			struct buddy *b;

			if ((b = gaim_find_buddy(gc->account, user)) != NULL)
				username = gaim_get_buddy_alias(b);
			else
				username = user;

			g_snprintf(msgbuf, sizeof(msgbuf),
					   _("%s has closed the conversation window"), username);

			if ((cnv = gaim_find_conversation(user)))
				gaim_conversation_write(cnv, NULL, msgbuf, -1,
							WFLAG_SYSTEM, time(NULL));

			msn_kill_switch(ms);
			return 0;
		}
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

			gaim_chat_add_user(GAIM_CHAT(ms->chat), user, NULL);
		} 
	} else if (!g_strncasecmp(buf, "JOI", 3)) {
		char *user, *tmp = buf;
		GET_NEXT(tmp);
		user = tmp;
		GET_NEXT(tmp);

		if (ms->total == 1) {
			ms->chat = serv_got_joined_chat(gc, ++id, "MSN Chat");
			gaim_chat_add_user(GAIM_CHAT(ms->chat), ms->user, NULL);
			gaim_chat_add_user(GAIM_CHAT(ms->chat), gc->username, NULL);
			g_free(ms->user);
			ms->user = NULL;
		}
		if (ms->chat)
			gaim_chat_add_user(GAIM_CHAT(ms->chat), user, NULL);
		ms->total++;
		while (ms->txqueue) {
			char *send = add_cr(ms->txqueue->data);
			g_snprintf(sendbuf, sizeof(sendbuf),
					   "MSG %u N %d\r\n%s%s", ++ms->trId,
					   strlen(MIME_HEADER) + strlen(send),
					   MIME_HEADER, send);

			g_free(ms->txqueue->data);
			ms->txqueue = g_slist_remove(ms->txqueue, ms->txqueue->data);

			if (msn_write(ms->fd, sendbuf, strlen(sendbuf)) < 0) {
				msn_kill_switch(ms);
				return 0;
			}

			debug_printf("\n");
		}
	} else if (!g_strncasecmp(buf, "MSG", 3)) {
		char *user, *tmp = buf;
		int length;

		GET_NEXT(tmp);
		user = tmp;

		GET_NEXT(tmp);

		GET_NEXT(tmp);
		length = atoi(tmp);

		ms->msg = TRUE;
		ms->msguser = g_strdup(user);
		ms->msglen = length;
	} else if (!g_strncasecmp(buf, "NAK", 3)) {
		do_error_dialog(_("An MSN message may not have been received."), NULL, GAIM_ERROR);
	} else if (!g_strncasecmp(buf, "NLN", 3)) {
	} else if (!g_strncasecmp(buf, "OUT", 3)) {
		if (ms->chat)
			serv_got_chat_left(gc, gaim_chat_get_id(GAIM_CHAT(ms->chat)));
		msn_kill_switch(ms);
		return 0;
	} else if (!g_strncasecmp(buf, "USR", 3)) {
		/* good, we got USR, now we need to find out who we want to talk to */
		struct msn_switchboard *ms = msn_find_writable_switch(gc);

		if (!ms)
			return 0;

		g_snprintf(sendbuf, sizeof(sendbuf), "CAL %u %s\r\n",
				   ++ms->trId, ms->user);

		if (msn_write(ms->fd, sendbuf, strlen(sendbuf)) < 0) {
			msn_kill_switch(ms);
			return 0;
		}
	} else if (isdigit(*buf)) {
		handle_errcode(buf, TRUE);

		if (atoi(buf) == 217)
			msn_kill_switch(ms);

	} else {
		debug_printf("Unhandled message!\n");
	}

	return 1;
}

static void
msn_process_switch_msg(struct msn_switchboard *ms, char *msg)
{
	char *content, *agent, *format;
	char *message = NULL;
	int flags = 0;

	agent = strstr(msg, "User-Agent: ");
	if (agent) {
		if (!g_strncasecmp(agent, "User-Agent: Gaim",
						   strlen("User-Agent: Gaim")))
			flags |= IM_FLAG_GAIMUSER;
	}

	format = strstr(msg, "X-MMS-IM-Format: ");
	if (format) {
		format = msn_parse_format(format);
	} else {
		format = NULL;
	}

	content = strstr(msg, "Content-Type: ");
	if (!content)
		return;
	if (!g_strncasecmp(content, "Content-Type: text/x-msmsgscontrol\r\n",
			   strlen(  "Content-Type: text/x-msmsgscontrol\r\n"))) {
		if (strstr(content,"TypingUser: ") && !ms->chat) {
			serv_got_typing(ms->gc, ms->msguser,
							MSN_TYPING_RECV_TIMEOUT, TYPING);
			return;
		} 

	} else if (!g_strncasecmp(content, "Content-Type: text/x-msmsgsinvite;",
							  strlen("Content-Type: text/x-msmsgsinvite;"))) {

		/*
		 * NOTE: Other things, such as voice communication, would go in
		 *       here too (since they send the same Content-Type). However,
		 *       this is the best check for file transfer messages, so I'm
		 *       calling msn_process_ft_invite_msg(). If anybody adds support
		 *       for anything else that sends a text/x-msmsgsinvite, perhaps
		 *       this should be changed. For now, it stays.
		 */
		msn_process_ft_msg(ms, content);

	} else if (!g_strncasecmp(content, "Content-Type: text/plain",
				  strlen("Content-Type: text/plain"))) {

		char *skiphead = strstr(msg, "\r\n\r\n");

		if (!skiphead || !skiphead[4]) {
			return;
		}

		skiphead += 4;
		strip_linefeed(skiphead);
		
		if (format) { 
			message = g_strdup_printf("%s%s", format, skiphead);
		} else {
			message = g_strdup(skiphead);
		}
		
		if (ms->chat)
			serv_got_chat_in(ms->gc, gaim_chat_get_id(GAIM_CHAT(ms->chat)),
							 ms->msguser, flags, message, time(NULL));
		else
			serv_got_im(ms->gc, ms->msguser, message, flags, time(NULL), -1);

		g_free(message);
	}
}

static void
msn_switchboard_callback(gpointer data, gint source, GaimInputCondition cond)
{
	struct msn_switchboard *ms = data;
	char buf[MSN_BUF_LEN];
	int cont = 1;
	int len;
	
	ms->fd = source;
	len = read(ms->fd, buf, sizeof(buf));
	if (len <= 0) {
		msn_kill_switch(ms);
		return;
	}

	ms->rxqueue = g_realloc(ms->rxqueue, len + ms->rxlen);
	memcpy(ms->rxqueue + ms->rxlen, buf, len);
	ms->rxlen += len;

	while (cont) {
		if (!ms->rxlen)
			return;

		if (ms->msg) {
			char *msg;
			if (ms->msglen > ms->rxlen)
				return;
			msg = ms->rxqueue;
			ms->rxlen -= ms->msglen;
			if (ms->rxlen) {
				ms->rxqueue = g_memdup(msg + ms->msglen, ms->rxlen);
			} else {
				ms->rxqueue = NULL;
				msg = g_realloc(msg, ms->msglen + 1);
			}
			msg[ms->msglen] = 0;
			ms->msglen = 0;
			ms->msg = FALSE;

			msn_process_switch_msg(ms, msg);

			g_free(ms->msguser);
			g_free(msg);
		} else {
			char *end = ms->rxqueue;
			int cmdlen;
			char *cmd;
			int i = 0;

			while (i + 1 < ms->rxlen) {
				if (*end == '\r' && end[1] == '\n')
					break;
				end++; i++;
			}
			if (i + 1 == ms->rxlen)
				return;

			cmdlen = end - ms->rxqueue + 2;
			cmd = ms->rxqueue;
			ms->rxlen -= cmdlen;
			if (ms->rxlen) {
				ms->rxqueue = g_memdup(cmd + cmdlen, ms->rxlen);
			} else {
				ms->rxqueue = NULL;
				cmd = g_realloc(cmd, cmdlen + 1);
			}
			cmd[cmdlen] = 0;

			debug_printf("MSN S: %s", cmd);
			g_strchomp(cmd);
			cont = msn_process_switch(ms, cmd);

			g_free(cmd);
		}
	}
}

void
msn_rng_connect(gpointer data, gint source, GaimInputCondition cond)
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

	g_snprintf(buf, sizeof(buf), "ANS %u %s %s %s\r\n", ++ms->trId, gc->username, ms->auth, ms->sessid);
	if (msn_write(ms->fd, buf, strlen(buf)) < 0) {
		close(ms->fd);
		g_free(ms->sessid);
		g_free(ms->auth);
		g_free(ms);
		return;
	}

	md->switches = g_slist_append(md->switches, ms);
	ms->inpa = gaim_input_add(ms->fd, GAIM_INPUT_READ,
							  msn_switchboard_callback, ms);
}

static void
msn_ss_xfr_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct msn_switchboard *ms = data;
	struct gaim_connection *gc = ms->gc;
	char buf[MSN_BUF_LEN];

	if (source == -1 || !g_slist_find(connections, gc)) {
		close(source);
		if (g_slist_find(connections, gc)) {
			msn_kill_switch(ms);
			do_error_dialog(_("Gaim was unable to send an MSN message"),
					_("Gaim encountered an error communicating with the "
					  "MSN switchboard server.  Please try again later."),
					GAIM_ERROR);
		}

		return;
	}

	if (ms->fd != source)
		ms->fd = source;

	g_snprintf(buf, sizeof(buf), "USR %u %s %s\r\n",
			   ++ms->trId, gc->username, ms->auth);

	if (msn_write(ms->fd, buf, strlen(buf)) < 0) {
		g_free(ms->auth);
		g_free(ms);
		return;
	}

	ms->inpa = gaim_input_add(ms->fd, GAIM_INPUT_READ,
							  msn_switchboard_callback, ms);
}

struct msn_switchboard *
msn_find_switch(struct gaim_connection *gc, const char *username)
{
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	GSList *m = md->switches;

	for (m = md->switches; m != NULL; m = m->next) {
		struct msn_switchboard *ms = (struct msn_switchboard *)m->data;

		if (ms->total <= 1 && !g_strcasecmp(ms->user, username))
			return ms;
	}

	return NULL;
}

struct msn_switchboard *
msn_find_switch_by_id(struct gaim_connection *gc, int chat_id)
{
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	GSList *m;

	for (m = md->switches; m != NULL; m = m->next) {
		struct msn_switchboard *ms = (struct msn_switchboard *)m->data;

		if (ms->chat && gaim_chat_get_id(GAIM_CHAT(ms->chat)) == chat_id)
			return ms;
	}

	return NULL;
}

struct msn_switchboard *
msn_find_writable_switch(struct gaim_connection *gc)
{
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	GSList *m;
	
	for (m = md->switches; m != NULL; m = m->next) {
		struct msn_switchboard *ms = (struct msn_switchboard *)m->data;

		if (ms->txqueue != NULL)
			return ms;
	}

	return NULL;
}

void
msn_kill_switch(struct msn_switchboard *ms)
{
	struct gaim_connection *gc = ms->gc;
	struct msn_data *md = gc->proto_data;

	if (ms->inpa)
		gaim_input_remove(ms->inpa);

	close(ms->fd);
	g_free(ms->rxqueue);

	if (ms->msg)    g_free(ms->msguser);
	if (ms->user)   g_free(ms->user);
	if (ms->sessid) g_free(ms->sessid);

	g_free(ms->auth);

	while (ms->txqueue) {
		g_free(ms->txqueue->data);
		ms->txqueue = g_slist_remove(ms->txqueue, ms->txqueue->data);
	}

	if (ms->chat)
		serv_got_chat_left(gc, gaim_chat_get_id(GAIM_CHAT(ms->chat)));

	md->switches = g_slist_remove(md->switches, ms);

	g_free(ms);
}

struct msn_switchboard *
msn_switchboard_connect(struct gaim_connection *gc, const char *host, int port)
{
	struct msn_switchboard *ms;

	if (host == NULL || port == 0)
		return NULL;

	ms = msn_find_writable_switch(gc);

	if (ms == NULL)
		return NULL;

	if (proxy_connect(gc->account, (char *)host, port, msn_ss_xfr_connect,
				ms) != 0) {
		msn_kill_switch(ms);

		return NULL;
	}

	return ms;
}
