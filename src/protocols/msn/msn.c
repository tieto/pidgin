/**
 * @file msn.c The MSN protocol plugin
 *
 * gaim
 *
 * Parts Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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

#define BUDDY_ALIAS_MAXLEN 388

static GaimPlugin *my_protocol = NULL;

/* for win32 compatability */
G_MODULE_IMPORT GSList *connections;

static void msn_login_callback(gpointer, gint, GaimInputCondition);
static void msn_login_xfr_connect(gpointer, gint, GaimInputCondition);

static char *msn_normalize(const char *s)
{
	static char buf[BUF_LEN];

	g_return_val_if_fail(s != NULL, NULL);

	g_snprintf(buf, sizeof(buf), "%s%s", s, strchr(s, '@') ? "" : "@hotmail.com");

	return buf;
}

char *
handle_errcode(char *buf, gboolean show)
{
	int errcode;
	static char msg[MSN_BUF_LEN];

	buf[4] = 0;
	errcode = atoi(buf);

	switch (errcode) {
		case 200:
			g_snprintf(msg, sizeof(msg), _("Syntax Error (probably a Gaim bug)"));
			break;
		case 201:
			g_snprintf(msg, sizeof(msg), _("Invalid Parameter (probably a Gaim bug)"));
			break;
		case 205:
			g_snprintf(msg, sizeof(msg), _("Invalid User"));
			break;
		case 206:
			g_snprintf(msg, sizeof(msg), _("Fully Qualified Domain Name missing"));
			break;
		case 207:
			g_snprintf(msg, sizeof(msg), _("Already Login"));
			break;
		case 208:
			g_snprintf(msg, sizeof(msg), _("Invalid Username"));
			break;
		case 209:
			g_snprintf(msg, sizeof(msg), _("Invalid Friendly Name"));
			break;
		case 210:
			g_snprintf(msg, sizeof(msg), _("List Full"));
			break;
		case 215:
			g_snprintf(msg, sizeof(msg), _("Already there"));
			break;
		case 216:
			g_snprintf(msg, sizeof(msg), _("Not on list"));
			break;
		case 217:
			g_snprintf(msg, sizeof(msg), _("User is offline"));
			break;
		case 218:
			g_snprintf(msg, sizeof(msg), _("Already in the mode"));
			break;
		case 219:
			g_snprintf(msg, sizeof(msg), _("Already in opposite list"));
			break;
		case 280:
			g_snprintf(msg, sizeof(msg), _("Switchboard failed"));
			break;
		case 281:
			g_snprintf(msg, sizeof(msg), _("Notify Transfer failed"));
			break;

		case 300:
			g_snprintf(msg, sizeof(msg), _("Required fields missing"));
			break;
		case 302:
			g_snprintf(msg, sizeof(msg), _("Not logged in"));
			break;

		case 500:
			g_snprintf(msg, sizeof(msg), _("Internal server error"));
			break;
		case 501:
			g_snprintf(msg, sizeof(msg), _("Database server error"));
			break;
		case 510:
			g_snprintf(msg, sizeof(msg), _("File operation error"));
			break;
		case 520:
			g_snprintf(msg, sizeof(msg), _("Memory allocation error"));
			break;

		case 600:
			g_snprintf(msg, sizeof(msg), _("Server busy"));
			break;
		case 601:
			g_snprintf(msg, sizeof(msg), _("Server unavailable"));
			break;
		case 602:
			g_snprintf(msg, sizeof(msg), _("Peer Notification server down"));
			break;
		case 603:
			g_snprintf(msg, sizeof(msg), _("Database connect error"));
			break;
		case 604:
			g_snprintf(msg, sizeof(msg), _("Server is going down (abandon ship)"));
			break;

		case 707:
			g_snprintf(msg, sizeof(msg), _("Error creating connection"));
			break;
		case 711:
			g_snprintf(msg, sizeof(msg), _("Unable to write"));
			break;
		case 712:
			g_snprintf(msg, sizeof(msg), _("Session overload"));
			break;
		case 713:
			g_snprintf(msg, sizeof(msg), _("User is too active"));
			break;
		case 714:
			g_snprintf(msg, sizeof(msg), _("Too many sessions"));
			break;
		case 715:
			g_snprintf(msg, sizeof(msg), _("Not expected"));
			break;
		case 717:
			g_snprintf(msg, sizeof(msg), _("Bad friend file"));
			break;

		case 911:
			g_snprintf(msg, sizeof(msg), _("Authentication failed"));
			break;
		case 913:
			g_snprintf(msg, sizeof(msg), _("Not allowed when offline"));
			break;
	        case 920:
			g_snprintf(msg, sizeof(msg), _("Not accepting new users"));
			break;
	        case 924:
			g_snprintf(msg, sizeof(msg), _("User unverified"));
			break;
		default:
			g_snprintf(msg, sizeof(msg), _("Unknown Error Code"));
			break;
	}

	if (show)
		do_error_dialog(msg, NULL, GAIM_ERROR);

	return msg;
}


char *url_decode(const char *msg)
{
	static char buf[MSN_BUF_LEN];
	int i, j = 0;
	char *bum;

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

	if(!g_utf8_validate(buf, -1, (const char **)&bum))
			*bum = '\0';

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

static void handle_hotmail(struct gaim_connection *gc, char *data)
{
	char login_url[2048];
	char buf[MSN_BUF_LEN];
	struct msn_data *md = gc->proto_data;

	if (strchr(gc->username, '@') != strstr(gc->username, "@hotmail.com"))
		/* We can only get Hotmail notification from hotmail users */
		return;

	if (!md->passport) {
		g_snprintf(buf, sizeof(buf), "URL %u INBOX\r\n", ++md->trId);

		if (msn_write(md->fd, buf, strlen(buf)) < 0) {
			return;
		}
	} else {
		g_snprintf(login_url, sizeof(login_url), "%s", md->passport);

		if (strstr(data, "Content-Type: text/x-msmsgsinitialemailnotification;")) {
			char *x = strstr(data, "Inbox-Unread:");
			if (!x) return;
			x += strlen("Inbox-Unread: ");
			connection_has_mail(gc, atoi(x), NULL, NULL, login_url);
		} else if (strstr(data, "Content-Type: text/x-msmsgsemailnotification;")) {
			char *from = strstr(data, "From:");
			char *subject = strstr(data, "Subject:");
			char *x;
			if (!from || !subject) {
				connection_has_mail(gc, 1, NULL, NULL, login_url);
				return;
			}
			from += strlen("From: ");
			x = strstr(from, "\r\n"); *x = 0;
			subject += strlen("Subject: ");
			x = strstr(subject, "\r\n"); *x = 0;
			connection_has_mail(gc, -1, from, subject, login_url);
		}
	}
}

struct msn_add_permit {
	struct gaim_connection *gc;
	char *user;
	char *friend;
};

static void msn_accept_add(struct msn_add_permit *map)
{
	if(g_slist_find(connections, map->gc)) {
		struct msn_data *md = map->gc->proto_data;
		char buf[MSN_BUF_LEN];

		g_snprintf(buf, sizeof(buf), "ADD %u AL %s %s\r\n", ++md->trId, map->user, url_encode(map->friend));

		if (msn_write(md->fd, buf, strlen(buf)) < 0) {
			hide_login_progress(map->gc, _("Write error"));
			signoff(map->gc);
			return;
		}
		gaim_privacy_permit_add(map->gc->account, map->user);
		build_allow_list(); /* er. right. we'll need to have a thing for this in CUI too */
		show_got_added(map->gc, NULL, map->user, map->friend, NULL);
	}

	g_free(map->user);
	g_free(map->friend);
	g_free(map);
}

static void msn_cancel_add(struct msn_add_permit *map)
{
	if(g_slist_find(connections, map->gc)) {
		struct msn_data *md = map->gc->proto_data;
		char buf[MSN_BUF_LEN];

		g_snprintf(buf, sizeof(buf), "ADD %u BL %s %s\r\n", ++md->trId, map->user, url_encode(map->friend));
		if (msn_write(md->fd, buf, strlen(buf)) < 0) {
			hide_login_progress(map->gc, _("Write error"));
			signoff(map->gc);
			return;
		}
		gaim_privacy_deny_add(map->gc->account, map->user);
		build_block_list();
	}

	g_free(map->user);
	g_free(map->friend);
	g_free(map);
}

static int msn_process_main(struct gaim_connection *gc, char *buf)
{
	struct msn_data *md = gc->proto_data;
	char sendbuf[MSN_BUF_LEN];

	if (!g_ascii_strncasecmp(buf, "ADD", 3)) {
		char *list, *user, *friend, *tmp = buf;
		struct msn_add_permit *ap;
		GSList *perm = gc->account->permit;
		char msg[MSN_BUF_LEN];

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		list = tmp;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		user = tmp;

		GET_NEXT(tmp);
		friend = url_decode(tmp);

		if (g_ascii_strcasecmp(list, "RL"))
			return 1;

		while (perm) {
			if (!gaim_utf8_strcasecmp(perm->data, user))
				return 1;
			perm = perm->next;
		}

		ap = g_new0(struct msn_add_permit, 1);
		ap->user = g_strdup(user);
		ap->friend = g_strdup(friend);
		ap->gc = gc;

		g_snprintf(msg, sizeof(msg), _("The user %s (%s) wants to add %s to his or her buddy list."),
				ap->user, ap->friend, ap->gc->username);

		//	do_ask_dialog(msg, NULL, ap, _("Authorize"), msn_accept_add, _("Deny"), msn_cancel_add, my_protocol->handle, FALSE);
	} else if (!g_ascii_strncasecmp(buf, "BLP", 3)) {
		char *type, *tmp = buf;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		GET_NEXT(tmp);
		type = tmp;

		if (!g_ascii_strcasecmp(type, "AL")) {
			/* If the current setting is AL, messages
			 * from users who are not in BL will be delivered
			 *
			 * In other words, deny some */
			gc->account->permdeny = DENY_SOME;
		} else {
			/* If the current
			 * setting is BL, only messages from people who are in the AL will be
			 * delivered.
			 *
			 * In other words, permit some */
			gc->account->permdeny = PERMIT_SOME;
		}
	} else if (!g_ascii_strncasecmp(buf, "BPR", 3)) {
	} else if (!g_ascii_strncasecmp(buf, "CHG", 3)) {
	} else if (!g_ascii_strncasecmp(buf, "CHL", 3)) {
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

		g_snprintf(sendbuf, sizeof(sendbuf), "QRY %u msmsgs@msnmsgr.com 32\r\n", ++md->trId);
		for (i = 0; i < 16; i++) {
			g_snprintf(buf2, sizeof(buf2), "%02x", di[i]);
			strcat(sendbuf, buf2);
		}

		if (msn_write(md->fd, sendbuf, strlen(sendbuf)) < 0) {
			hide_login_progress(gc, _("Unable to write to server"));
			signoff(gc);
		}

		debug_printf("\n");
	} else if (!g_ascii_strncasecmp(buf, "FLN", 3)) {
		char *usr = buf;

		GET_NEXT(usr);
		serv_got_update(gc, usr, 0, 0, 0, 0, 0);
	} else if (!g_ascii_strncasecmp(buf, "GTC", 3)) {
	} else if (!g_ascii_strncasecmp(buf, "INF", 3)) {
	} else if (!g_ascii_strncasecmp(buf, "ILN", 3)) {
		char *state, *user, *friend, *tmp = buf;
		int status = 0;

		GET_NEXT(tmp);

		GET_NEXT(tmp);
		state = tmp;

		GET_NEXT(tmp);
		user = tmp;

		GET_NEXT(tmp);
		friend = url_decode(tmp);

		serv_got_alias(gc, user, friend);

		if (!g_ascii_strcasecmp(state, "BSY")) {
			status |= UC_UNAVAILABLE | (MSN_BUSY << 1);
		} else if (!g_ascii_strcasecmp(state, "IDL")) {
			status |= UC_UNAVAILABLE | (MSN_IDLE << 1);
		} else if (!g_ascii_strcasecmp(state, "BRB")) {
			status |= UC_UNAVAILABLE | (MSN_BRB << 1);
		} else if (!g_ascii_strcasecmp(state, "AWY")) {
			status |= UC_UNAVAILABLE | (MSN_AWAY << 1);
		} else if (!g_ascii_strcasecmp(state, "PHN")) {
			status |= UC_UNAVAILABLE | (MSN_PHONE << 1);
		} else if (!g_ascii_strcasecmp(state, "LUN")) {
			status |= UC_UNAVAILABLE | (MSN_LUNCH << 1);
		}

		serv_got_update(gc, user, 1, 0, 0, 0, status);
	} else if (!g_ascii_strncasecmp(buf, "LST", 3)) {
		char *which, *who, *friend, *tmp = buf;
		struct msn_add_permit *ap; /* for any as yet undealt with buddies who've added you to their buddy list when you were off-line.  How dare they! */
		GSList *perm = gc->account->permit; /* current permit list */
		GSList *denyl = gc->account->deny;
		char msg[MSN_BUF_LEN];
		int new = 1;
		int pos, tot;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		which = tmp;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		pos = strtol(tmp, NULL, 10);
		GET_NEXT(tmp);
		tot = strtol(tmp, NULL, 10);
		GET_NEXT(tmp);
		who = tmp;

		GET_NEXT(tmp);
		friend = url_decode(tmp);

		if (!g_ascii_strcasecmp(which, "FL") && pos) {
			struct msn_buddy *b = g_new0(struct msn_buddy, 1);
			b->user = g_strdup(who);
			b->friend = g_strdup(friend);
			md->fl = g_slist_append(md->fl, b);
		} else if (!g_ascii_strcasecmp(which, "AL") && pos) {
			if (g_slist_find_custom(gc->account->deny, who,
							(GCompareFunc)strcmp)) {
				debug_printf("moving from deny to permit: %s", who);
				gaim_privacy_deny_remove(gc->account, who);
			}
			gaim_privacy_permit_add(gc->account, who);
		} else if (!g_ascii_strcasecmp(which, "BL") && pos) {
			gaim_privacy_deny_add(gc->account, who);
		} else if (!g_ascii_strcasecmp(which, "RL")) {
		    if (pos) {
			while(perm) {
				if(!g_ascii_strcasecmp(perm->data, who))
					new = 0;
				perm = perm->next;
			}
			while(denyl) {
			  if(!g_ascii_strcasecmp(denyl->data, who))
			    new = 0;
			  denyl = denyl->next;
			}
			if(new) {
				debug_printf("Unresolved MSN RL entry\n");
				ap = g_new0(struct msn_add_permit, 1);
				ap->user = g_strdup(who);
				ap->friend = g_strdup(friend);
				ap->gc = gc;
                         
				g_snprintf(msg, sizeof(msg), _("The user %s (%s) wants to add you to their buddy list"),ap->user, ap->friend);
				do_ask_dialog(msg, NULL, ap, _("Authorize"), msn_accept_add, _("Deny"), msn_cancel_add, my_protocol->handle, FALSE);
			}
		    }
			
			if (pos != tot) 
				return 1; /* this isn't the last one in the RL, so return. */

			g_snprintf(sendbuf, sizeof(sendbuf), "CHG %u NLN\r\n", ++md->trId);
			if (msn_write(md->fd, sendbuf, strlen(sendbuf)) < 0) {
				hide_login_progress(gc, _("Unable to write"));
				signoff(gc);
				return 0;
			}

			account_online(gc);
			serv_finish_login(gc);

			md->permit = g_slist_copy(gc->account->permit);
			md->deny = g_slist_copy(gc->account->deny);

			while (md->fl) {
				struct msn_buddy *mb = md->fl->data;
				struct buddy *b = gaim_find_buddy(gc->account, mb->user);
				md->fl = g_slist_remove(md->fl, mb);
				if(!b) {
					struct group *g;
					printf("I'm adding %s now..\n", mb->user);
					if (!(g = gaim_find_group(_("Buddies")))) {
						printf("How could I not exitst!?!\n");
						g = gaim_group_new(_("Buddies"));
						gaim_blist_add_group(g, NULL);
					}
					b = gaim_buddy_new(gc->account, mb->user, NULL);
					gaim_blist_add_buddy(b,g,NULL);
				}
				serv_got_alias(gc, mb->user, mb->friend);
				g_free(mb->user);
				g_free(mb->friend);
				g_free(mb);
			}
		}
	} else if (!g_ascii_strncasecmp(buf, "MSG", 3)) {
		char *user, *tmp = buf;
		int length;

		GET_NEXT(tmp);
		user = tmp;

		GET_NEXT(tmp);

		GET_NEXT(tmp);
		length = atoi(tmp);

		md->msg = TRUE;
		md->msguser = g_strdup(user);
		md->msglen = length;
	} else if (!g_ascii_strncasecmp(buf, "NLN", 3)) {
		char *state, *user, *friend, *tmp = buf;
		int status = 0;

		GET_NEXT(tmp);
		state = tmp;

		GET_NEXT(tmp);
		user = tmp;

		GET_NEXT(tmp);
		friend = url_decode(tmp);

		serv_got_alias(gc, user, friend);

		if (!g_ascii_strcasecmp(state, "BSY")) {
			status |= UC_UNAVAILABLE | (MSN_BUSY << 1);
		} else if (!g_ascii_strcasecmp(state, "IDL")) {
			status |= UC_UNAVAILABLE | (MSN_IDLE << 1);
		} else if (!g_ascii_strcasecmp(state, "BRB")) {
			status |= UC_UNAVAILABLE | (MSN_BRB << 1);
		} else if (!g_ascii_strcasecmp(state, "AWY")) {
			status |= UC_UNAVAILABLE | (MSN_AWAY << 1);
		} else if (!g_ascii_strcasecmp(state, "PHN")) {
			status |= UC_UNAVAILABLE | (MSN_PHONE << 1);
		} else if (!g_ascii_strcasecmp(state, "LUN")) {
			status |= UC_UNAVAILABLE | (MSN_LUNCH << 1);
		}

		serv_got_update(gc, user, 1, 0, 0, 0, status);
	} else if (!g_ascii_strncasecmp(buf, "OUT", 3)) {
		char *tmp = buf;

		GET_NEXT(tmp);
		if (!g_ascii_strncasecmp(tmp, "OTH", 3)) {
			hide_login_progress(gc, _("You have been disconnected. You have "
						  "signed on from another location."));
			signoff(gc);
			return 0;
		}
	} else if (!g_ascii_strncasecmp(buf, "PRP", 3)) {
	} else if (!g_ascii_strncasecmp(buf, "QNG", 3)) {
	} else if (!g_ascii_strncasecmp(buf, "QRY", 3)) {
	} else if (!g_ascii_strncasecmp(buf, "REA", 3)) {
		char *friend, *tmp = buf;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		GET_NEXT(tmp);
		GET_NEXT(tmp);

		friend = url_decode(tmp);

		g_snprintf(gc->displayname, sizeof(gc->displayname), "%s", friend);
	} else if (!g_ascii_strncasecmp(buf, "REM", 3)) {
	} else if (!g_ascii_strncasecmp(buf, "RNG", 3)) {
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
		if (proxy_connect(gc->account, ssaddr, port, msn_rng_connect, ms) != 0) {
			g_free(ms);
			return 1;
		}
		ms->user = g_strdup(user);
		ms->sessid = g_strdup(sessid);
		ms->auth = g_strdup(auth);
		ms->gc = gc;
	} else if (!g_ascii_strncasecmp(buf, "URL", 3)) {
		char *tmp = buf;
		FILE *fd;
		md5_state_t st;
		md5_byte_t di[16];
		int i;
		char buf2[64];
		char sendbuf[64];
		char hippy[2048];
		char *rru;
		char *passport;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		rru = tmp;
		GET_NEXT(tmp);
		passport = tmp;
		
		g_snprintf(hippy, sizeof(hippy), "%s%lu%s", md->mspauth, time(NULL) - md->sl, gc->password);

		md5_init(&st);
		md5_append(&st, (const md5_byte_t *)hippy, strlen(hippy));
		md5_finish(&st, di);

		bzero(sendbuf, sizeof(sendbuf));
		for (i = 0; i < 16; i++) {
			g_snprintf(buf2, sizeof(buf2), "%02x", di[i]);
			strcat(sendbuf, buf2);
		}

		if (md->passport) {
			unlink(md->passport);
			g_free(md->passport);
		}

		if( (fd = gaim_mkstemp(&(md->passport))) == NULL ) {
		  debug_printf("Error opening temp file: %s\n", strerror(errno)); 
		}
		else {
			fputs("<html>\n"
				"<head>\n"
				"<noscript>\n"
				"<meta http-equiv=Refresh content=\"0; url=http://www.hotmail.com\">\n"
				"</noscript>\n"
				"</head>\n\n", fd);
		
		        fprintf(fd, "<body onload=\"document.pform.submit(); \">\n");
		        fprintf(fd, "<form name=\"pform\" action=\"%s\" method=\"POST\">\n\n", passport);
		        fprintf(fd, "<input type=\"hidden\" name=\"mode\" value=\"ttl\">\n");
		        fprintf(fd, "<input type=\"hidden\" name=\"login\" value=\"%s\">\n", gc->username);
		        fprintf(fd, "<input type=\"hidden\" name=\"username\" value=\"%s\">\n", gc->username);
			fprintf(fd, "<input type=\"hidden\" name=\"sid\" value=\"%s\">\n", md->sid);
			fprintf(fd, "<input type=\"hidden\" name=\"kv\" value=\"%s\">\n", md->kv);
			fprintf(fd, "<input type=\"hidden\" name=\"id\" value=\"2\">\n");
			fprintf(fd, "<input type=\"hidden\" name=\"sl\" value=\"%ld\">\n", time(NULL) - md->sl);
			fprintf(fd, "<input type=\"hidden\" name=\"rru\" value=\"%s\">\n", rru);
			fprintf(fd, "<input type=\"hidden\" name=\"auth\" value=\"%s\">\n", md->mspauth);
			fprintf(fd, "<input type=\"hidden\" name=\"creds\" value=\"%s\">\n", sendbuf); // Digest me
			fprintf(fd, "<input type=\"hidden\" name=\"svc\" value=\"mail\">\n");
			fprintf(fd, "<input type=\"hidden\" name=\"js\" value=\"yes\">\n");
			fprintf(fd, "</form></body>\n");
			fprintf(fd, "</html>\n");
			if (fclose(fd)) {
				debug_printf("Error closing temp file: %s\n", strerror(errno)); 
				unlink(md->passport);
				g_free(md->passport);
			}
			else {
				/*
				 * Renaming file with .html extension, so that
				 * win32 open_url will work.
				 */
				char *tmp;
				if ((tmp = g_strdup_printf("%s.html", md->passport)) != NULL) {
					if (rename(md->passport, tmp) == 0) {
						g_free(md->passport);
						md->passport = tmp;
					} else
						g_free(tmp);
				}
			}
		}
	} else if (!g_ascii_strncasecmp(buf, "SYN", 3)) {
	} else if (!g_ascii_strncasecmp(buf, "USR", 3)) {
	} else if (!g_ascii_strncasecmp(buf, "XFR", 3)) {
		char *host = strstr(buf, "SB");
		int port;
		int i = 0;
		gboolean switchboard = TRUE;
		char *tmp;

		if (!host) {
			host = strstr(buf, "NS");
			if (!host) {
				hide_login_progress(gc, _("Got invalid XFR\n"));
				signoff(gc);
				return 0;
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
			struct msn_switchboard *ms;

			GET_NEXT(tmp);

			ms = msn_switchboard_connect(gc, host, port);

			if (ms == NULL)
				return 1;

			ms->auth = g_strdup(tmp);
		} else {
			close(md->fd);
			gaim_input_remove(md->inpa);
			md->inpa = 0;
			if (proxy_connect(gc->account, host, port, msn_login_xfr_connect, gc) != 0) {
				hide_login_progress(gc, _("Error transferring"));
				signoff(gc);
				return 0;
			}
		}
	} else if (isdigit(*buf)) {
		handle_errcode(buf, TRUE);
	} else {
		debug_printf("Unhandled message!\n");
	}

	return 1;
}

static void msn_process_main_msg(struct gaim_connection *gc, char *msg)
{
	struct msn_data *md = gc->proto_data;
	char *skiphead;
	char *content;

	content = strstr(msg, "Content-Type: ");

	if ((content) && (!g_ascii_strncasecmp(content, "Content-Type: text/x-msmsgsprofile",
				strlen("Content-Type: text/x-msmsgsprofile")))) {

		char *kv,*sid,*mspauth;

		kv = strstr(msg, "kv: ");
		sid = strstr(msg, "sid: ");
		mspauth = strstr(msg, "MSPAuth: ");

		if (kv) {
			char *tmp;

			kv += strlen("kv: ");
			tmp = strstr(kv, "\r\n"); *tmp = 0;
			md->kv = g_strdup(kv);
		}

		if (sid) {
			char *tmp;

			sid += strlen("sid: ");
			tmp = strstr(sid, "\r\n"); *tmp = 0;
			md->sid = g_strdup(sid);
		}

		if (mspauth) {
			char *tmp;

			mspauth += strlen("MSPAuth: ");
			tmp = strstr(mspauth, "\r\n"); *tmp = 0;
			md->mspauth = g_strdup(mspauth);
		}

	}



	if (!g_ascii_strcasecmp(md->msguser, "hotmail")) {
		handle_hotmail(gc, msg);
		return;
	}


	skiphead = strstr(msg, "\r\n\r\n");
	if (!skiphead || !skiphead[4])
		return;
	skiphead += 4;
	strip_linefeed(skiphead);

	serv_got_im(gc, md->msguser, skiphead, 0, time(NULL), -1);
}

static void msn_callback(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];
	int cont = 1;
	int len;

	len = read(md->fd, buf, sizeof(buf));
	if (len <= 0) {
		hide_login_progress_error(gc, _("Error reading from server"));
		signoff(gc);
		return;
	}

	md->rxqueue = g_realloc(md->rxqueue, len + md->rxlen);
	memcpy(md->rxqueue + md->rxlen, buf, len);
	md->rxlen += len;

	while (cont) {
		if (!md->rxlen)
			return;

		if (md->msg) {
			char *msg;
			if (md->msglen > md->rxlen)
				return;
			msg = md->rxqueue;
			md->rxlen -= md->msglen;
			if (md->rxlen) {
				md->rxqueue = g_memdup(msg + md->msglen, md->rxlen);
			} else {
				md->rxqueue = NULL;
				msg = g_realloc(msg, md->msglen + 1);
			}
			msg[md->msglen] = 0;
			md->msglen = 0;
			md->msg = FALSE;

			msn_process_main_msg(gc, msg);

			g_free(md->msguser);
			g_free(msg);
		} else {
			char *end = md->rxqueue;
			int cmdlen;
			char *cmd;
			int i = 0;

			while (i + 1 < md->rxlen) {
				if (*end == '\r' && end[1] == '\n')
					break;
				end++; i++;
			}
			if (i + 1 == md->rxlen)
				return;

			cmdlen = end - md->rxqueue + 2;
			cmd = md->rxqueue;
			md->rxlen -= cmdlen;
			if (md->rxlen) {
				md->rxqueue = g_memdup(cmd + cmdlen, md->rxlen);
			} else {
				md->rxqueue = NULL;
				cmd = g_realloc(cmd, cmdlen + 1);
			}
			cmd[cmdlen] = 0;

			debug_printf("MSN S: %s", cmd);
			g_strchomp(cmd);
			cont = msn_process_main(gc, cmd);

			g_free(cmd);
		}
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
		hide_login_progress(gc, _("Unable to connect to Notification Server"));
		signoff(gc);
		return;
	}

	g_snprintf(buf, sizeof(buf), "VER %u MSNP5\r\n", ++md->trId);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Unable to talk to Notification Server"));
		signoff(gc);
		return;
	}

	md->inpa = gaim_input_add(md->fd, GAIM_INPUT_READ, msn_login_callback, gc);
}

static int msn_process_login(struct gaim_connection *gc, char *buf)
{
	struct msn_data *md = gc->proto_data;
	char sendbuf[MSN_BUF_LEN];

	if (!g_ascii_strncasecmp(buf, "VER", 3)) {
		/* we got VER, check to see that MSNP5 is in the list, then send INF */
		if (!strstr(buf, "MSNP5")) {
			hide_login_progress(gc, _("Protocol not supported"));
			signoff(gc);
			return 0;
		}

		g_snprintf(sendbuf, sizeof(sendbuf), "INF %u\r\n", ++md->trId);
		if (msn_write(md->fd, sendbuf, strlen(sendbuf)) < 0) {
			hide_login_progress(gc, _("Unable to request INF\n"));
			signoff(gc);
			return 0;
		}
	} else if (!g_ascii_strncasecmp(buf, "INF", 3)) {
		/* check to make sure we can use md5 */
		if (!strstr(buf, "MD5")) {
			hide_login_progress(gc, _("Unable to login using MD5"));
			signoff(gc);
			return 0;
		}

		g_snprintf(sendbuf, sizeof(sendbuf), "USR %u MD5 I %s\r\n", ++md->trId, gc->username);
		if (msn_write(md->fd, sendbuf, strlen(sendbuf)) < 0) {
			hide_login_progress(gc, _("Unable to send USR\n"));
			signoff(gc);
			return 0;
		}

		set_login_progress(gc, 3, _("Requesting to send password"));
	} else if (!g_ascii_strncasecmp(buf, "USR", 3)) {
		char *resp, *friend, *tmp = buf;

		GET_NEXT(tmp);
		GET_NEXT(tmp);
		resp = tmp;
		GET_NEXT(tmp);
		GET_NEXT(tmp);
		friend = url_decode(tmp);
		GET_NEXT(tmp);

		/* so here, we're either getting the challenge or the OK */
		if (!g_ascii_strcasecmp(resp, "OK")) {
			g_snprintf(gc->displayname, sizeof(gc->displayname), "%s", friend);

			g_snprintf(sendbuf, sizeof(sendbuf), "SYN %u 0\r\n", ++md->trId);
			if (msn_write(md->fd, sendbuf, strlen(sendbuf)) < 0) {
				hide_login_progress(gc, _("Unable to write"));
				signoff(gc);
				return 0;
			}

			gaim_input_remove(md->inpa);
			md->inpa = gaim_input_add(md->fd, GAIM_INPUT_READ, msn_callback, gc);
			return 0;
		} else if (!g_ascii_strcasecmp(resp, "MD5")) {
			char buf2[MSN_BUF_LEN];
			md5_state_t st;
			md5_byte_t di[16];
			int i;

			g_snprintf(buf2, sizeof(buf2), "%s%s", friend, gc->password);

			md5_init(&st);
			md5_append(&st, (const md5_byte_t *)buf2, strlen(buf2));
			md5_finish(&st, di);

			g_snprintf(sendbuf, sizeof(sendbuf), "USR %u MD5 S ", ++md->trId);
			for (i = 0; i < 16; i++) {
				g_snprintf(buf2, sizeof(buf2), "%02x", di[i]);
				strcat(sendbuf, buf2);
			}
			strcat(sendbuf, "\r\n");

			if (msn_write(md->fd, sendbuf, strlen(sendbuf)) < 0) {
				hide_login_progress(gc, _("Unable to send password"));
				signoff(gc);
				return 0;
			}

			set_login_progress(gc, 4, _("Password sent"));
		}
	} else if (!g_ascii_strncasecmp(buf, "XFR", 3)) {
		char *host = strstr(buf, "NS");
		int port;
		int i = 0;

		if (!host) {
			hide_login_progress(gc, _("Got invalid XFR\n"));
			signoff(gc);
			return 0;
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
		md->sl = time(NULL);
		if (proxy_connect(gc->account, host, port, msn_login_xfr_connect, gc) != 0) {
			hide_login_progress(gc, _("Unable to transfer"));
			signoff(gc);
		}
		return 0;
	} else {
		if (isdigit(*buf))
			hide_login_progress(gc, handle_errcode(buf, FALSE));
		else
			hide_login_progress(gc, _("Unable to parse message"));
		signoff(gc);
		return 0;
	}

	return 1;
}

static void msn_login_callback(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];
	int cont = 1;
	int len;

	len = read(md->fd, buf, sizeof(buf));
	if (len <= 0) {
		hide_login_progress(gc, _("Error reading from server"));
		signoff(gc);
		return;
	}

	md->rxqueue = g_realloc(md->rxqueue, len + md->rxlen);
	memcpy(md->rxqueue + md->rxlen, buf, len);
	md->rxlen += len;

	while (cont) {
		char *end = md->rxqueue;
		int cmdlen;
		char *cmd;
		int i = 0;

		if (!md->rxlen)
			return;

		while (i + 1 < md->rxlen) {
			if (*end == '\r' && end[1] == '\n')
				break;
			end++; i++;
		}
		if (i + 1 == md->rxlen)
			return;

		cmdlen = end - md->rxqueue + 2;
		cmd = md->rxqueue;
		md->rxlen -= cmdlen;
		if (md->rxlen) {
			md->rxqueue = g_memdup(cmd + cmdlen, md->rxlen);
		} else {
			md->rxqueue = NULL;
			cmd = g_realloc(cmd, cmdlen + 1);
		}
		cmd[cmdlen] = 0;

		debug_printf("MSN S: %s", cmd);
		g_strchomp(cmd);
		cont = msn_process_login(gc, cmd);

		g_free(cmd);
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
		hide_login_progress(gc, _("Unable to connect"));
		signoff(gc);
		return;
	}

	g_snprintf(buf, sizeof(buf), "VER %u MSNP5\r\n", ++md->trId);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Unable to write to server"));
		signoff(gc);
		return;
	}

	md->inpa = gaim_input_add(md->fd, GAIM_INPUT_READ, msn_login_callback, gc);
	set_login_progress(gc, 2,_("Synching with server"));
}

static void msn_login(struct gaim_account *account)
{
	struct gaim_connection *gc = new_gaim_conn(account);
	gc->proto_data = g_new0(struct msn_data, 1);

	set_login_progress(gc, 1, _("Connecting"));

	g_snprintf(gc->username, sizeof(gc->username), "%s", msn_normalize(gc->username));

	if (proxy_connect(account, account->proto_opt[USEROPT_MSNSERVER][0] ?
				account->proto_opt[USEROPT_MSNSERVER] : MSN_SERVER,
				account->proto_opt[USEROPT_MSNPORT][0] ?
				atoi(account->proto_opt[USEROPT_MSNPORT]) : MSN_PORT,
				msn_login_connect, gc) != 0) {
		hide_login_progress(gc, _("Unable to connect"));
		signoff(gc);
	}
}

static void msn_close(struct gaim_connection *gc)
{
	struct msn_data *md = gc->proto_data;
	close(md->fd);
	if (md->inpa)
		gaim_input_remove(md->inpa);
	g_free(md->rxqueue);
	if (md->msg)
		g_free(md->msguser);
	if (md->passport) {
		unlink(md->passport);
      		g_free(md->passport);
	}
	while (md->switches)
		msn_kill_switch(md->switches->data);
	while (md->fl) {
		struct msn_buddy *tmp = md->fl->data;
		md->fl = g_slist_remove(md->fl, tmp);
		g_free(tmp->user);
		g_free(tmp->friend);
		g_free(tmp);
	}
	g_slist_free(md->permit);
	g_slist_free(md->deny);
	g_free(md);
}

static int msn_send_typing(struct gaim_connection *gc, char *who, int typing) {
	struct msn_switchboard *ms = msn_find_switch(gc, who);
	char header[MSN_BUF_LEN] =   "MIME-Version: 1.0\r\n"
				     "Content-Type: text/x-msmsgscontrol\r\n" 
				     "TypingUser: ";
	char buf [MSN_BUF_LEN];
	if (!ms || !typing)
		return 0;
	g_snprintf(buf, sizeof(buf), "MSG %u N %d\r\n%s%s\r\n\r\n\r\n",
		   ++ms->trId,
		   strlen(header) + strlen("\r\n\r\n\r\n") + strlen(gc->username),
		   header, gc->username);
	if (msn_write(ms->fd, buf, strlen(buf)) < 0)
	           msn_kill_switch(ms);
	return MSN_TYPING_SEND_TIMEOUT;
}

static int msn_send_im(struct gaim_connection *gc, const char *who, const char *message, int len, int flags)
{
	struct msn_data *md = gc->proto_data;
	struct msn_switchboard *ms = msn_find_switch(gc, who);
	char buf[MSN_BUF_LEN];

	if (ms) {
		char *send;

		if (ms->txqueue) {
			debug_printf("appending to queue\n");
			ms->txqueue = g_slist_append(ms->txqueue, g_strdup(message));
			return 1;
		}

		send = add_cr(message);
		g_snprintf(buf, sizeof(buf), "MSG %u N %d\r\n%s%s", ++ms->trId,
				strlen(MIME_HEADER) + strlen(send),
				MIME_HEADER, send);
		g_free(send);
		if (msn_write(ms->fd, buf, strlen(buf)) < 0)
			msn_kill_switch(ms);
		debug_printf("\n");
	} else if (strcmp(who, gc->username)) {
		g_snprintf(buf, MSN_BUF_LEN, "XFR %u SB\r\n", ++md->trId);
		if (msn_write(md->fd, buf, strlen(buf)) < 0) {
			hide_login_progress(gc, _("Write error"));
			signoff(gc);
			return 1;
		}

		ms = g_new0(struct msn_switchboard, 1);
		md->switches = g_slist_append(md->switches, ms);
		ms->user = g_strdup(who);
		ms->txqueue = g_slist_append(ms->txqueue, g_strdup(message));
		ms->gc = gc;
		ms->fd = -1;
	} else
		/* in msn you can't send messages to yourself, so we'll fake like we received it ;) */
		serv_got_im(gc, who, message, flags | IM_FLAG_GAIMUSER, time(NULL), -1);
	return 1;
}

static int msn_chat_send(struct gaim_connection *gc, int id, char *message)
{
	struct msn_switchboard *ms = msn_find_switch_by_id(gc, id);
	char buf[MSN_BUF_LEN];
	char *send;

	if (!ms)
		return -EINVAL;

	send = add_cr(message);
	g_snprintf(buf, sizeof(buf), "MSG %u N %d\r\n%s%s", ++ms->trId,
			strlen(MIME_HEADER) + strlen(send),
			MIME_HEADER, send);
	g_free(send);
	if (msn_write(ms->fd, buf, strlen(buf)) < 0) {
		msn_kill_switch(ms);
		return 0;
	}
	debug_printf("\n");
	serv_got_chat_in(gc, id, gc->username, 0, message, time(NULL));
	return 0;
}

static void msn_chat_invite(struct gaim_connection *gc, int id, const char *msg, const char *who)
{
	struct msn_switchboard *ms = msn_find_switch_by_id(gc, id);
	char buf[MSN_BUF_LEN];

	if (!ms)
		return;

	g_snprintf(buf, sizeof(buf), "CAL %u %s\r\n", ++ms->trId, who);
	if (msn_write(ms->fd, buf, strlen(buf)) < 0)
		msn_kill_switch(ms);
}

static void msn_chat_leave(struct gaim_connection *gc, int id)
{
	struct msn_switchboard *ms = msn_find_switch_by_id(gc, id);
	char buf[MSN_BUF_LEN];

	if (!ms)
		return;

	g_snprintf(buf, sizeof(buf), "OUT\r\n");
	if (msn_write(ms->fd, buf, strlen(buf)) < 0)
		msn_kill_switch(ms);
}

static GList *msn_away_states(struct gaim_connection *gc)
{
	GList *m = NULL;

	m = g_list_append(m, _("Available"));
	m = g_list_append(m, _("Away From Computer"));
	m = g_list_append(m, _("Be Right Back"));
	m = g_list_append(m, _("Busy"));
	m = g_list_append(m, _("On The Phone"));
	m = g_list_append(m, _("Out To Lunch"));
	m = g_list_append(m, _("Hidden"));

	return m;
}

static void msn_set_away(struct gaim_connection *gc, char *state, char *msg)
{
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];
	const char *away;

	if (gc->away) {
		g_free(gc->away);
		gc->away = NULL;
	}

	if (msg) {
		gc->away = g_strdup("");
		away = "AWY";
	} else if (state) {
		gc->away = g_strdup("");

		if (!strcmp(state, _("Away From Computer")))
			away = "AWY";
		else if (!strcmp(state, _("Be Right Back")))
			away = "BRB";
		else if (!strcmp(state, _("Busy")))
			away = "BSY";
		else if (!strcmp(state, _("On The Phone")))
			away = "PHN";
		else if (!strcmp(state, _("Out To Lunch")))
			away = "LUN";
		else if (!strcmp(state, _("Hidden")))
			away = "HDN";
		else {
			g_free(gc->away);
			gc->away = NULL;
			away = "NLN";
		}
	} else if (gc->is_idle)
		away = "IDL";
	else
		away = "NLN";

	g_snprintf(buf, sizeof(buf), "CHG %u %s\r\n", ++md->trId, away);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void msn_set_idle(struct gaim_connection *gc, int idle)
{
	struct msn_data *md = gc->proto_data;
	char buf[64];

	if (gc->away)
		return;
	g_snprintf(buf, sizeof(buf),
		idle ? "CHG %d IDL\r\n" : "CHG %u NLN\r\n", ++md->trId);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static const char *msn_list_icon(struct gaim_account *a, struct buddy *b)
{
	return "msn";
}

static void msn_list_emblems(struct buddy *b, char **se, char **sw, char **nw, char **ne)
{
	if (b->present == GAIM_BUDDY_OFFLINE)
		*se = "offline";
	else if (b->uc >> 1 == 2 || b->uc >> 1 == 6)
		*se = "occupied";
	else if (b->uc)
		*se = "away";
}

static char *msn_get_away_text(int s)
{
	switch (s) {
		case MSN_BUSY :
			return _("Busy");
		case MSN_BRB :
			return _("Be Right Back");
		case MSN_AWAY :
			return _("Away From Computer");
		case MSN_PHONE :
			return _("On The Phone");
		case MSN_LUNCH :
			return _("Out To Lunch");
		case MSN_IDLE :
			return _("Idle");
		default:
			return _("Available");
	}
}

static char *msn_status_text(struct buddy *b) {
	if (b->uc & UC_UNAVAILABLE)
		return g_strdup(msn_get_away_text(b->uc >> 1));
	return NULL;
}

static char *msn_tooltip_text(struct buddy *b) {
	if (GAIM_BUDDY_IS_ONLINE(b))
		return g_strdup_printf(_("<b>Status:</b> %s"), msn_get_away_text(b->uc >> 1));

	return NULL;
}

static GList *msn_buddy_menu(struct gaim_connection *gc, const char *who)
{
	GList *m = NULL;

	return m;
}

static void msn_add_buddy(struct gaim_connection *gc, const char *name)
{
	struct msn_data *md = gc->proto_data;
	char *who = msn_normalize(name);
	char buf[MSN_BUF_LEN];
	GSList *l = md->fl;

	if (who[0] == '@')
		/* how did this happen? */
		return;

	if (strchr(who, ' '))
		/* This is a broken blist entry. */
		return;

	while (l) {
		struct msn_buddy *b = l->data;
		if (!gaim_utf8_strcasecmp(who, b->user))
			break;
		l = l->next;
	}
	if (l)
		return;

	g_snprintf(buf, sizeof(buf), "ADD %u FL %s %s\r\n", ++md->trId, who, who);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void msn_rem_buddy(struct gaim_connection *gc, char *who, char *group)
{
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];

	g_snprintf(buf, sizeof(buf), "REM %u FL %s\r\n", ++md->trId, who);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void msn_act_id(gpointer data, char *entry)
{
	struct gaim_connection *gc = data;
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];
	char *alias;

	if (!entry || *entry == '\0') 
		alias = g_strdup("");
	else
		alias = g_strdup(entry);
	
	if (strlen(alias) >= BUDDY_ALIAS_MAXLEN) {
		do_error_dialog(_("New MSN friendly name too long."), NULL, GAIM_ERROR);
		return;
	}
	
	g_snprintf(buf, sizeof(buf), "REA %u %s %s\r\n", ++md->trId, gc->username, url_encode(alias));
	g_free(alias);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void msn_show_set_friendly_name(struct gaim_connection *gc)
{
	do_prompt_dialog(_("Set Friendly Name:"), gc->displayname, gc, msn_act_id, NULL);
}

static GList *msn_actions(struct gaim_connection *gc)
{
	GList *m = NULL;
	struct proto_actions_menu *pam;

	pam = g_new0(struct proto_actions_menu, 1);
	pam->label = _("Set Friendly Name");
	pam->callback = msn_show_set_friendly_name;
	pam->gc = gc;
	m = g_list_append(m, pam);

	return m;
}

static void msn_convo_closed(struct gaim_connection *gc, char *who)
{
	struct msn_switchboard *ms = msn_find_switch(gc, who);

	if (ms) {
		char sendbuf[256];

		g_snprintf(sendbuf, sizeof(sendbuf), "BYE %s\r\n", gc->username);

		msn_write(ms->fd, sendbuf, strlen(sendbuf));

		msn_kill_switch(ms);
	}
}

static void msn_keepalive(struct gaim_connection *gc)
{
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];

	g_snprintf(buf, sizeof(buf), "PNG\r\n");
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void msn_set_permit_deny(struct gaim_connection *gc)
{
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];
	GSList *s, *t = NULL;

	if (gc->account->permdeny == PERMIT_ALL || gc->account->permdeny == DENY_SOME)
		g_snprintf(buf, sizeof(buf), "BLP %u AL\r\n", ++md->trId);
	else
		g_snprintf(buf, sizeof(buf), "BLP %u BL\r\n", ++md->trId);

	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}

	/* this is safe because we'll always come here after we've gotten the list off the server,
	 * and data is never removed. So if the lengths are equal we don't know about anyone locally
	 * and so there's no sense in going through them all. */
	if (g_slist_length(gc->account->permit) == g_slist_length(md->permit)) {
		g_slist_free(md->permit);
		md->permit = NULL;
	}
	if (g_slist_length(gc->account->deny) == g_slist_length(md->deny)) {
		g_slist_free(md->deny);
		md->deny = NULL;
	}
	if (!md->permit && !md->deny)
		return;

	if (md->permit) {
		s = g_slist_nth(gc->account->permit, g_slist_length(md->permit));
		while (s) {
			char *who = s->data;
			s = s->next;
			if (!strchr(who, '@')) {
				t = g_slist_append(t, who);
				continue;
			}
			if (g_slist_find(md->deny, who)) {
				t = g_slist_append(t, who);
				continue;
			}
			g_snprintf(buf, sizeof(buf), "ADD %u AL %s %s\r\n", ++md->trId, who, who);
			if (msn_write(md->fd, buf, strlen(buf)) < 0) {
				hide_login_progress(gc, _("Write error"));
				signoff(gc);
				return;
			}
		}
		while (t) {
			gaim_privacy_permit_remove(gc->account, t->data);
			t = t->next;
		}
		if (t)
			g_slist_free(t);
	t = NULL;
	g_slist_free(md->permit);
	md->permit = NULL;
	}
	
	if (md->deny) {
		s = g_slist_nth(gc->account->deny, g_slist_length(md->deny));
		while (s) {
			char *who = s->data;
			s = s->next;
			if (!strchr(who, '@')) {
				t = g_slist_append(t, who);
				continue;
			}
			if (g_slist_find(md->deny, who)) {
				t = g_slist_append(t, who);
				continue;
			}
			g_snprintf(buf, sizeof(buf), "ADD %u BL %s %s\r\n", ++md->trId, who, who);
			if (msn_write(md->fd, buf, strlen(buf)) < 0) {
				hide_login_progress(gc, _("Write error"));
				signoff(gc);
				return;
			}
		}
		while (t) {
			gaim_privacy_deny_remove(gc->account, t->data);
			t = t->next;
		}
		if (t)
			g_slist_free(t);
	g_slist_free(md->deny);
	md->deny = NULL;
	}
}

static void msn_add_permit(struct gaim_connection *gc, const char *who)
{
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];

	if (!strchr(who, '@')) {
		g_snprintf(buf, sizeof(buf), 
			   _("An MSN screenname must be in the form \"user@server.com\".  "
			     "Perhaps you meant %s@hotmail.com.  No changes were made to your "
			     "allow list."), who);
		do_error_dialog(_("Invalid MSN screenname"), buf, GAIM_ERROR);
		gaim_privacy_permit_remove(gc->account, who);
		return;
	}

	if (g_slist_find_custom(gc->account->deny, who, (GCompareFunc)strcmp)) {
		debug_printf("MSN: Moving %s from BL to AL\n", who);
		gaim_privacy_deny_remove(gc->account, who);
		g_snprintf(buf, sizeof(buf), "REM %u BL %s\r\n", ++md->trId, who);
			if (msn_write(md->fd, buf, strlen(buf)) < 0) {
				hide_login_progress(gc, _("Write error"));
				signoff(gc);
				return;
			}
	}
	g_snprintf(buf, sizeof(buf), "ADD %u AL %s %s\r\n", ++md->trId, who, who);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void msn_rem_permit(struct gaim_connection *gc, const char *who)
{
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];

	g_snprintf(buf, sizeof(buf), "REM %u AL %s\r\n", ++md->trId, who);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}

	gaim_privacy_deny_add(gc->account, who);
	g_snprintf(buf, sizeof(buf), "ADD %u BL %s %s\r\n", ++md->trId, who, who);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void msn_add_deny(struct gaim_connection *gc, const char *who)
{
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];

	if (!strchr(who, '@')) {
		g_snprintf(buf, sizeof(buf), 
			   _("An MSN screenname must be in the form \"user@server.com\".  "
			     "Perhaps you meant %s@hotmail.com.  No changes were made to your "
			     "block list."), who);
		do_error_dialog(_("Invalid MSN screenname"), buf, GAIM_ERROR);
		gaim_privacy_deny_remove(gc->account, who);
		return;
	}

	if (g_slist_find_custom(gc->account->permit, who, (GCompareFunc)strcmp)) {
		debug_printf("MSN: Moving %s from AL to BL\n", who);
		gaim_privacy_permit_remove(gc->account, who);
		g_snprintf(buf, sizeof(buf), "REM %u AL %s\r\n", ++md->trId, who);
		if (msn_write(md->fd, buf, strlen(buf)) < 0) {
			hide_login_progress(gc, _("Write error"));
			signoff(gc);
			return;
		}
	}


	g_snprintf(buf, sizeof(buf), "ADD %u BL %s %s\r\n", ++md->trId, who, who);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void msn_rem_deny(struct gaim_connection *gc, const char *who)
{
	struct msn_data *md = gc->proto_data;
	char buf[MSN_BUF_LEN];

	g_snprintf(buf, sizeof(buf), "REM %u BL %s\r\n", ++md->trId, who);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}

	gaim_privacy_permit_add(gc->account, who);
	g_snprintf(buf, sizeof(buf), "ADD %u AL %s %s\r\n", ++md->trId, who, who);
	if (msn_write(md->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void msn_buddy_free(struct buddy *b)
{
	if (b->proto_data)
		g_free(b->proto_data);
}

static GaimPluginProtocolInfo prpl_info =
{
	GAIM_PROTO_MSN,
	OPT_PROTO_MAIL_CHECK,
	NULL,
	NULL,
	msn_list_icon,
	msn_list_emblems,
	msn_status_text,
	msn_tooltip_text,
	msn_away_states,
	msn_actions,
	msn_buddy_menu,
	NULL,
	msn_login,
	msn_close,
	msn_send_im,
	NULL,
	msn_send_typing,
	NULL,
	msn_set_away,
	NULL,
	NULL,
	NULL,
	NULL,
	msn_set_idle,
	NULL,
	msn_add_buddy,
	NULL,
	msn_rem_buddy,
	NULL,
	msn_add_permit,
	msn_add_deny,
	msn_rem_permit,
	msn_rem_deny,
	msn_set_permit_deny,
	NULL,
	NULL,
	msn_chat_invite,
	msn_chat_leave,
	NULL,
	msn_chat_send,
	msn_keepalive,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	msn_buddy_free,
	msn_convo_closed,
	msn_normalize
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-msn",                                       /**< id             */
	"MSN",                                            /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("MSN Protocol Plugin"),
	                                                  /**  description    */
	N_("MSN Protocol Plugin"),
	"Christian Hammond <chipx86@gnupdate.org>",       /**< author         */
	WEBSITE,                                          /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info                                        /**< extra_info     */
};

static void
__init_plugin(GaimPlugin *plugin)
{
	struct proto_user_opt *puo;

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = g_strdup(_("Login Server:"));
	puo->def = g_strdup(MSN_SERVER);
	puo->pos = USEROPT_MSNSERVER;
	prpl_info.user_opts = g_list_append(prpl_info.user_opts, puo);

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = g_strdup(_("Port:"));
	puo->def = g_strdup("1863");
	puo->pos = USEROPT_MSNPORT;
	prpl_info.user_opts = g_list_append(prpl_info.user_opts, puo);

	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(msn, __init_plugin, info);
