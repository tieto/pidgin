/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * gaim
 *
 * Copyright (C) 1998-2001, Mark Spencer <markster@marko.net>
 * Some code borrowed from GtkZephyr, by
 * Jag/Sean Dilda <agrajag@linuxpower.org>/<smdilda@unity.ncsu.edu>
 * http://gtkzephyr.linuxpower.org/
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
/* XXX eww */
#include "src/internal.h"

#include "accountopt.h"
#include "debug.h"
#include "multi.h"
#include "notify.h"
#include "prpl.h"
#include "server.h"
#include "util.h"

#include "zephyr/zephyr.h"

#include <strings.h>

#define ZEPHYR_FALLBACK_CHARSET "ISO-8859-1"

extern Code_t ZGetLocations(ZLocations_t *, int *);
extern Code_t ZSetLocation(char *);
extern Code_t ZUnsetLocation();

typedef struct _zframe zframe;
typedef struct _zephyr_triple zephyr_triple;

/* struct I need for zephyr_to_html */
struct _zframe {
	/* true for everything but @color, since inside the parens of that one is
	 * the color. */
	gboolean has_closer;
	/* </i>, </font>, </b>, etc. */
	char *closing;
	/* text including the opening html thingie. */
	GString *text;
	struct _zframe *enclosing;
};

struct _zephyr_triple {
	char *class;
	char *instance;
	char *recipient;
	char *name;
	gboolean open;
	int id;
};

#define z_call(func)		if (func != ZERR_NONE)\
					return;
#define z_call_r(func)		if (func != ZERR_NONE)\
					return TRUE;
#define z_call_s(func, err)	if (func != ZERR_NONE) {\
					gaim_connection_error(zgc, err);\
					return;\
				}

char *local_zephyr_normalize(const char *);
static const char *zephyr_normalize(const GaimAccount *, const char *);
static const char *gaim_zephyr_get_realm();

char *zephyr_strip_foreign_realm(const char* user){
/*
	Takes in a username of the form username or username@realm 
	and returns:
	username, if there is no realm, or the realm is the local realm
	or:
	username@realm if there is a realm and it is foreign
*/
	char *tmp = g_strdup(user);
	char *at = strchr(tmp,'@');
	if (at && !g_ascii_strcasecmp(at+1,gaim_zephyr_get_realm())) {
		/* We're passed in a username of the form user@users-realm */
		char* tmp2;
		*at = '\0';
		tmp2 = g_strdup(tmp);
		g_free(tmp);
		return tmp2;
	}
	else {
		/* We're passed in a username of the form user or user@foreign-realm */
		return tmp;
	}
}

/* this is so bad, and if Zephyr weren't so fucked up to begin with I
 * wouldn't do this. but it is so i will. */
static guint32 nottimer = 0;
static guint32 loctimer = 0;
GaimConnection *zgc = NULL;
static GList *pending_zloc_names = NULL;
static GSList *subscrips = NULL;
static int last_id = 0;

/* just for debugging */
static void handle_unknown(ZNotice_t notice)
{
	gaim_debug(GAIM_DEBUG_MISC, "zephyr","z_packet: %s\n", notice.z_packet);
	gaim_debug(GAIM_DEBUG_MISC, "zephyr","z_version: %s\n", notice.z_version);
	gaim_debug(GAIM_DEBUG_MISC, "zephyr","z_kind: %d\n", (int)(notice.z_kind));
	gaim_debug(GAIM_DEBUG_MISC, "zephyr","z_class: %s\n", notice.z_class);
	gaim_debug(GAIM_DEBUG_MISC, "zephyr","z_class_inst: %s\n", notice.z_class_inst);
	gaim_debug(GAIM_DEBUG_MISC, "zephyr","z_opcode: %s\n", notice.z_opcode);
	gaim_debug(GAIM_DEBUG_MISC, "zephyr","z_sender: %s\n", notice.z_sender);
	gaim_debug(GAIM_DEBUG_MISC, "zephyr","z_recipient: %s\n", notice.z_recipient);
	gaim_debug(GAIM_DEBUG_MISC, "zephyr","z_message: %s\n", notice.z_message);
	gaim_debug(GAIM_DEBUG_MISC, "zephyr","z_message_len: %d\n", notice.z_message_len);
}


static zephyr_triple *new_triple(const char *c, const char *i, const char *r)
{
	zephyr_triple *zt;

	zt = g_new0(zephyr_triple, 1);
	zt->class = g_strdup(c);
	zt->instance = g_strdup(i);
	zt->recipient = g_strdup(r);
	zt->name = g_strdup_printf("%s,%s,%s", c, i, r);
	zt->id = ++last_id;
	zt->open = FALSE;
	return zt;
}

static void free_triple(zephyr_triple * zt)
{
	g_free(zt->class);
	g_free(zt->instance);
	g_free(zt->recipient);
	g_free(zt->name);
	g_free(zt);
}

static const char *gaim_zephyr_get_sender()
{
	/* will be useful once this plugin can use a backend other
	   than libzephyr */
	return ZGetSender();
}

static const char *gaim_zephyr_get_realm()
{
	/* will be useful once this plugin can use a backend other
	   than libzephyr */
	return ZGetRealm();
}

/* returns true if zt1 is a subset of zt2.  This function is used to
   determine whether a zephyr sent to zt1 should be placed in the chat
   with triple zt2
 
   zt1 is a subset of zt2 
   iff. the classnames are identical ignoring case 
   AND. the instance names are identical (ignoring case), or zt2->instance is *.
   AND. the recipient names are identical
*/

static gboolean triple_subset(zephyr_triple * zt1, zephyr_triple * zt2)
{
	if (g_ascii_strcasecmp(zt2->class, zt1->class)) {
		return FALSE;
	}
	if (g_ascii_strcasecmp(zt2->instance, zt1->instance) && g_ascii_strcasecmp(zt2->instance, "*")) {
		return FALSE;
	}
	if (g_ascii_strcasecmp(zt2->recipient, zt1->recipient)) {
		return FALSE;
	}
	return TRUE;
}

static zephyr_triple *find_sub_by_triple(zephyr_triple * zt)
{
	zephyr_triple *curr_t;
	GSList *curr = subscrips;

	while (curr) {
		curr_t = curr->data;
		if (triple_subset(zt, curr_t))
			return curr_t;
		curr = curr->next;
	}
	return NULL;
}

static zephyr_triple *find_sub_by_id(int id)
{
	zephyr_triple *zt;
	GSList *curr = subscrips;

	while (curr) {
		zt = curr->data;
		if (zt->id == id)
			return zt;
		curr = curr->next;
	}
	return NULL;
}

/* 
   Convert incoming zephyr to utf-8 if necessary using user specified encoding
*/

static gchar *zephyr_recv_convert(char *string, int len)
{
	gchar *utf8;
	GError *err = NULL;

	if (g_utf8_validate(string, len, NULL)) {
		return g_strdup(string);
	} else {
		utf8 = g_convert(string, len, "UTF-8", gaim_account_get_string(zgc->account, "encoding", ZEPHYR_FALLBACK_CHARSET), NULL, NULL, &err);
		if (err) {
			gaim_debug(GAIM_DEBUG_ERROR, "zephyr", "recv conversion error: %s\n", err->message);
			utf8 = g_strdup(_("(There was an error converting this message.  Check the 'Encoding' option in the Account Editor)"));
			g_error_free(err);
		}

		return utf8;
	}
}

/* utility macros that are useful for zephyr_to_html */

#define IS_OPENER(c) ((c == '{') || (c == '[') || (c == '(') || (c == '<'))
#define IS_CLOSER(c) ((c == '}') || (c == ']') || (c == ')') || (c == '>'))

/* This parses HTML formatting (put out by one of the gtkimhtml widgets 
   And converts it to zephyr formatting.
   It currently deals properly with <b>, <br>, <i>, <font face=...>, <font color=...>,
   It ignores <font back=...>
   It does
   <font size = "1 or 2" -> @small
   3 or 4  @medium()
   5,6, or 7 @large()
   <a href is dealt with by ignoring the description and outputting the link
*/

static char *html_to_zephyr(const char *message)
{
	int len, cnt, retcount;
	char *ret;

	len = strlen(message);
	ret = g_new0(char, len * 3);

	bzero(ret, len * 3);
	retcount = 0;
	cnt = 0;
	while (cnt <= len) {
		if (message[cnt] == '<') {
			if (!g_ascii_strncasecmp(message + cnt + 1, "i>", 2)) {
				strncpy(ret + retcount, "@i(", 3);
				cnt += 3;
				retcount += 3;
			} else if (!g_ascii_strncasecmp(message + cnt + 1, "b>", 2)) {
				strncpy(ret + retcount, "@b(", 3);
				cnt += 3;
				retcount += 3;
			} else if (!g_ascii_strncasecmp(message + cnt + 1, "br>", 3)) {
				strncpy(ret + retcount, "\n", 1);
				cnt += 4;
				retcount += 1;
			} else if (!g_ascii_strncasecmp(message + cnt + 1, "a href=\"", 8)) {
				cnt += 9;
				while (g_ascii_strncasecmp(message + cnt, "\">", 2) != 0) {
					ret[retcount] = message[cnt];
					retcount++;
					cnt++;
				}
				cnt += 2;
				/* ignore descriptive string */
				while (g_ascii_strncasecmp(message + cnt, "</a>", 4) != 0) {
					cnt++;
				}
				cnt += 4;
			} else if (!g_ascii_strncasecmp(message + cnt + 1, "font", 4)) {
				cnt += 5;
				while (!g_ascii_strncasecmp(message + cnt, " ", 1))
					cnt++;
				if (!g_ascii_strncasecmp(message + cnt, "color=\"", 7)) {
					cnt += 7;
					strncpy(ret + retcount, "@color(", 7);
					retcount += 7;
					while (g_ascii_strncasecmp(message + cnt, "\">", 2) != 0) {
						ret[retcount] = message[cnt];
						retcount++;
						cnt++;
					}
					ret[retcount] = ')';
					retcount++;
					cnt += 2;
				} else if (!g_ascii_strncasecmp(message + cnt, "face=\"", 6)) {
					cnt += 6;
					strncpy(ret + retcount, "@font(", 6);
					retcount += 6;
					while (g_ascii_strncasecmp(message + cnt, "\">", 2) != 0) {
						ret[retcount] = message[cnt];
						retcount++;
						cnt++;
					}
					ret[retcount] = ')';
					retcount++;
					cnt += 2;
				} else if (!g_ascii_strncasecmp(message + cnt, "size=\"", 6)) {
					cnt += 6;
					if ((message[cnt] == '1') || (message[cnt] == '2')) {
						strncpy(ret + retcount, "@small(", 7);
						retcount += 7;
					} else if ((message[cnt] == '3')
							   || (message[cnt] == '4')) {
						strncpy(ret + retcount, "@medium(", 8);
						retcount += 8;
					} else if ((message[cnt] == '5')
							   || (message[cnt] == '6')
							   || (message[cnt] == '7')) {
						strncpy(ret + retcount, "@large(", 7);
						retcount += 7;
					}
					cnt += 3;
				} else {
					/* Drop all unrecognized/misparsed font tags */
					while (g_ascii_strncasecmp(message + cnt, "\">", 2) != 0) {
						cnt++;
					}
					cnt += 2;
				}
			} else if (!g_ascii_strncasecmp(message + cnt + 1, "/i>", 3)
					   || !g_ascii_strncasecmp(message + cnt + 1, "/b>", 3)) {
				cnt += 4;
				ret[retcount] = ')';
				retcount++;
			} else if (!g_ascii_strncasecmp(message + cnt + 1, "/font>", 6)) {
				cnt += 7;
				strncpy(ret + retcount, "@font(fixed)", 12);
				retcount += 12;
			} else {
				/* Catch all for all unrecognized/misparsed <foo> tage */
				while (g_ascii_strncasecmp(message + cnt, ">", 1) != 0) {
					ret[retcount] = message[cnt];
					retcount++;
					cnt++;
				}
			}
		} else {
			/* Duh */
			ret[retcount] = message[cnt];
			retcount++;
			cnt++;
		}
	}
	return ret;
}

/* this parses zephyr formatting and converts it to html. For example, if
 * you pass in "@{@color(blue)@i(hello)}" you should get out
 * "<font color=blue><i>hello</i></font>". */
static char *zephyr_to_html(char *message)
{
	int len, cnt;
	zframe *frames, *curr;
	char *ret;

	frames = g_new(zframe, 1);
	frames->text = g_string_new("");
	frames->enclosing = NULL;
	frames->closing = "";
	frames->has_closer = FALSE;

	len = strlen(message);
	cnt = 0;
	while (cnt <= len) {
		if (message[cnt] == '@') {
			zframe *new_f;
			char *buf;
			int end;

			for (end = 1; (cnt + end) <= len && !IS_OPENER(message[cnt + end])
				 && !IS_CLOSER(message[cnt + end]); end++);
			buf = g_new0(char, end);

			if (end) {
				g_snprintf(buf, end, "%s", message + cnt + 1);
			}
			if (!g_ascii_strcasecmp(buf, "italic") || !g_ascii_strcasecmp(buf, "i")) {
				new_f = g_new(zframe, 1);
				new_f->enclosing = frames;
				new_f->text = g_string_new("<i>");
				new_f->closing = "</i>";
				new_f->has_closer = TRUE;
				frames = new_f;
				cnt += end + 1;	/* cnt points to char after opener */
			} else if (!g_ascii_strcasecmp(buf, "small")) {
				new_f = g_new(zframe, 1);
				new_f->enclosing = frames;
				new_f->text = g_string_new("<font size=\"1\">");
				new_f->closing = "</font>";
				frames = new_f;
				cnt += end + 1;
			} else if (!g_ascii_strcasecmp(buf, "medium")) {
				new_f = g_new(zframe, 1);
				new_f->enclosing = frames;
				new_f->text = g_string_new("<font size=\"3\">");
				new_f->closing = "</font>";
				frames = new_f;
				cnt += end + 1;
			} else if (!g_ascii_strcasecmp(buf, "large")) {
				new_f = g_new(zframe, 1);
				new_f->enclosing = frames;
				new_f->text = g_string_new("<font size=\"7\">");
				new_f->closing = "</font>";
				frames = new_f;
				cnt += end + 1;
			} else if (!g_ascii_strcasecmp(buf, "bold")
					   || !g_ascii_strcasecmp(buf, "b")) {
				new_f = g_new(zframe, 1);
				new_f->enclosing = frames;
				new_f->text = g_string_new("<b>");
				new_f->closing = "</b>";
				new_f->has_closer = TRUE;
				frames = new_f;
				cnt += end + 1;
			} else if (!g_ascii_strcasecmp(buf, "font")) {
				cnt += end + 1;
				new_f = g_new(zframe, 1);
				new_f->enclosing = frames;
				new_f->text = g_string_new("<font face=");
				for (; (cnt <= len) && !IS_CLOSER(message[cnt]); cnt++) {
					g_string_append_c(new_f->text, message[cnt]);
				}
				cnt++;			/* point to char after closer */
				g_string_append_c(new_f->text, '>');
				new_f->closing = "</font>";
				new_f->has_closer = FALSE;
				frames = new_f;
			} else if (!g_ascii_strcasecmp(buf, "color")) {
				cnt += end + 1;
				new_f = g_new(zframe, 1);
				new_f->enclosing = frames;
				new_f->text = g_string_new("<font color=");
				for (; (cnt <= len) && !IS_CLOSER(message[cnt]); cnt++) {
					g_string_append_c(new_f->text, message[cnt]);
				}
				cnt++;			/* point to char after closer */
				g_string_append_c(new_f->text, '>');
				new_f->closing = "</font>";
				new_f->has_closer = FALSE;
				frames = new_f;
			} else if (!g_ascii_strcasecmp(buf, "")) {
				new_f = g_new(zframe, 1);
				new_f->enclosing = frames;
				new_f->text = g_string_new("");
				new_f->closing = "";
				new_f->has_closer = TRUE;
				frames = new_f;
				cnt += end + 1;	/* cnt points to char after opener */
			} else {
				if ((cnt + end) > len) {
					g_string_append_c(frames->text, '@');
					cnt++;
				} else if (IS_CLOSER(message[cnt + end])) {
					/* We have @chars..closer . This is 
					   merely a sequence of chars that isn't a formatting tag
					 */
					int tmp = cnt;

					while (tmp <= cnt + end) {
						g_string_append_c(frames->text, message[tmp]);
						tmp++;
					}
					cnt += end + 1;
				} else {
					/* unrecognized thingie. act like it's not there, but we
					 * still need to take care of the corresponding closer,
					 * make a frame that does nothing. */
					new_f = g_new(zframe, 1);
					new_f->enclosing = frames;
					new_f->text = g_string_new("");
					new_f->closing = "";
					new_f->has_closer = TRUE;
					frames = new_f;
					cnt += end + 1;	/* cnt points to char after opener */
				}
			}
		} else if (IS_CLOSER(message[cnt])) {
			zframe *popped;
			gboolean last_had_closer;

			if (frames->enclosing) {
				do {
					popped = frames;
					frames = frames->enclosing;
					g_string_append(frames->text, popped->text->str);
					g_string_append(frames->text, popped->closing);
					g_string_free(popped->text, TRUE);
					last_had_closer = popped->has_closer;
					g_free(popped);
				} while (frames && frames->enclosing && !last_had_closer);
			} else {
				g_string_append_c(frames->text, message[cnt]);
			}
			cnt++;
		} else if (message[cnt] == '\n') {
			g_string_append(frames->text, "<br>");
			cnt++;
		} else {
			g_string_append_c(frames->text, message[cnt++]);
		}
	}
	/* go through all the stuff that they didn't close */
	while (frames->enclosing) {
		curr = frames;
		g_string_append(frames->enclosing->text, frames->text->str);
		g_string_append(frames->enclosing->text, frames->closing);
		g_string_free(frames->text, TRUE);
		frames = frames->enclosing;
		g_free(curr);
	}
	ret = frames->text->str;
	g_string_free(frames->text, FALSE);
	g_free(frames);
	return ret;
}

static gboolean pending_zloc(char *who)
{
	GList *curr;

	for (curr = pending_zloc_names; curr != NULL; curr = curr->next) {
		char* normalized_who = local_zephyr_normalize(who);
		if (!g_ascii_strcasecmp(normalized_who, (char *)curr->data)) {
			g_free((char *)curr->data);
			pending_zloc_names = g_list_remove(pending_zloc_names, curr->data);
			return TRUE;
		}
	}
	return FALSE;
}

/* Called when the server notifies us a message couldn't get sent */

static void message_failed(ZNotice_t notice, struct sockaddr_in from)
{
	if (g_ascii_strcasecmp(notice.z_class, "message")) {
		gchar* chat_failed = g_strdup_printf(_("Unable send to chat %s,%s,%s"),notice.z_class,notice.z_class_inst,notice.z_recipient);
		gaim_notify_error(zgc,"",chat_failed,NULL);
		g_free(chat_failed);
	} else {
		gaim_notify_error(zgc, notice.z_recipient, _("User is offline"), NULL);
	}
}

static void handle_message(ZNotice_t notice, struct sockaddr_in from)
{
	if (!g_ascii_strcasecmp(notice.z_class, LOGIN_CLASS)) {
		/* well, we'll be updating in 20 seconds anyway, might as well ignore this. */
	} else if (!g_ascii_strcasecmp(notice.z_class, LOCATE_CLASS)) {
		if (!g_ascii_strcasecmp(notice.z_opcode, LOCATE_LOCATE)) {
			int nlocs;
			char *user;
			GaimBuddy *b;

			if (ZParseLocations(&notice, NULL, &nlocs, &user) != ZERR_NONE)
				return;

			if ((b = gaim_find_buddy(zgc->account, user)) == NULL) {
				char *e = strchr(user, '@');

				if (e && !g_ascii_strcasecmp(e + 1, gaim_zephyr_get_realm())) {
					*e = '\0';
				}
				b = gaim_find_buddy(zgc->account, user);
			}
			if ((b && pending_zloc(b->name)) || pending_zloc(user)) {
				ZLocations_t locs;
				int one = 1;
				GString *str = g_string_new("");

				g_string_append_printf(str, _("<b>User:</b> %s<br>"), b ? b->name : user);
				if (b && b->alias)
					g_string_append_printf(str, _("<b>Alias:</b> %s<br>"), b->alias);
				if (!nlocs) {
					g_string_append_printf(str, _("<br>Hidden or not logged-in"));
				}
				for (; nlocs > 0; nlocs--) {
					ZGetLocations(&locs, &one);
					g_string_append_printf(str, _("<br>At %s since %s"), locs.host, locs.time);
				}
				gaim_notify_formatted(zgc, NULL, _("Buddy Information"), NULL, str->str, NULL, NULL);
				g_string_free(str, TRUE);
			} else
				serv_got_update(zgc, b->name, nlocs, 0, 0, 0, 0);

			free(user);
		}
	} else {
		char *buf, *buf2, *buf3;
		char *send_inst;
		GaimConversation *gconv1;
		GaimConvChat *gcc;
		char *ptr = notice.z_message + strlen(notice.z_message) + 1;
		int len = notice.z_message_len - (strlen(notice.z_message) +1);
		char *sendertmp = g_strdup_printf("%s", gaim_zephyr_get_sender());
		GaimConvImFlags flags = 0;

		if (len > 0) {
			gchar *tmpescape;

			buf = g_malloc(len + 1);
			g_snprintf(buf, len + 1, "%s", ptr);
			g_strchomp(buf);
			tmpescape = gaim_escape_html(buf);
			buf2 = zephyr_to_html(tmpescape);
			buf3 = zephyr_recv_convert(buf2, strlen(buf2));
			g_free(buf2);
			g_free(buf);
			g_free(tmpescape);

			if (!g_ascii_strcasecmp(notice.z_class, "MESSAGE") && !g_ascii_strcasecmp(notice.z_class_inst, "PERSONAL") 
                            && !g_ascii_strcasecmp(notice.z_recipient,gaim_zephyr_get_sender())) {
				gchar* stripped_sender;
				if (!g_ascii_strcasecmp(notice.z_message, "Automated reply:"))
					flags |= GAIM_CONV_IM_AUTO_RESP;
				stripped_sender = zephyr_strip_foreign_realm(notice.z_sender);
				serv_got_im(zgc, stripped_sender, buf3, flags, time(NULL));
				g_free(stripped_sender);
			} else {
				zephyr_triple *zt1, *zt2;

				zt1 = new_triple(notice.z_class, notice.z_class_inst, notice.z_recipient);
				zt2 = find_sub_by_triple(zt1);
				if (!zt2) {
					/* we shouldn't be subscribed to this message. ignore. */
				} else {
					GList *gltmp;
					int found = 0;

					if (!zt2->open) {
						zt2->open = TRUE;
						serv_got_joined_chat(zgc, zt2->id, zt2->name);
						gconv1 = gaim_find_conversation_with_account(zt2->name, zgc->account);
						gcc = gaim_conversation_get_chat_data(gconv1);
						gaim_conv_chat_set_topic(gcc, sendertmp, notice.z_class_inst);

					}
					g_free(sendertmp); /* fix memory leak? */
					/* If the person is in the default Realm, then strip the 
					   Realm from the sender field */
					sendertmp = zephyr_strip_foreign_realm(notice.z_sender);
					send_inst = g_strdup_printf("%s %s",sendertmp,notice.z_class_inst);					
					serv_got_chat_in(zgc, zt2->id, send_inst, FALSE, buf3, time(NULL));

					gconv1 = gaim_find_conversation_with_account(zt2->name, zgc->account);
					gcc = gaim_conversation_get_chat_data(gconv1);
					/*                                        gaim_conv_chat_set_topic(gcc,sendertmp,notice.z_class_inst);  */
					for (gltmp = gaim_conv_chat_get_users(gcc); gltmp; gltmp = gltmp->next) {
						if (!g_ascii_strcasecmp(gltmp->data, sendertmp))
							found = 1;
					}
					if (!found) {
						/* force interpretation in network byte order */
						unsigned char *addrs = (unsigned char *)&(notice.z_sender_addr.s_addr);
						gchar* ipaddr = g_strdup_printf("%hhd.%hhd.%hhd.%hhd", (unsigned char)addrs[0], 
										(unsigned char)addrs[1], (unsigned char)addrs[2], 
										(unsigned char) addrs[3]);

						gaim_conv_chat_add_user(gcc, sendertmp, ipaddr);
						g_free(ipaddr); /* fix memory leak? */

					}
					g_free(sendertmp);
					g_free(send_inst);
				}
				free_triple(zt1);
			}
			g_free(buf3);
		}
	}
}

static gint check_notify(gpointer data)
{
	while (ZPending()) {
		ZNotice_t notice;
		struct sockaddr_in from;

		z_call_r(ZReceiveNotice(&notice, &from));

		switch (notice.z_kind) {
		case UNSAFE:
		case UNACKED:
		case ACKED:
			handle_message(notice, from);
			break;
		case SERVACK:
			if (!(g_ascii_strcasecmp(notice.z_message, ZSRVACK_NOTSENT))) {
				message_failed(notice, from);
			}
			break;
		case CLIENTACK:
			gaim_debug(GAIM_DEBUG_ERROR,"zephyr", "Client ack received\n");
		default:
			/* we'll just ignore things for now */
			handle_unknown(notice);
			gaim_debug(GAIM_DEBUG_ERROR, "zephyr", "Unhandled notice.\n");
			break;
		}

		ZFreeNotice(&notice);
	}

	return TRUE;
}

static gint check_loc(gpointer data)
{
	GaimBlistNode *gnode, *cnode, *bnode;
	ZAsyncLocateData_t ald;

	ald.user = NULL;
	memset(&(ald.uid), 0, sizeof(ZUnique_Id_t));
	ald.version = NULL;

	for (gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for (cnode = gnode->child; cnode; cnode = cnode->next) {
			if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;
			for (bnode = cnode->child; bnode; bnode = bnode->next) {
				GaimBuddy *b = (GaimBuddy *) bnode;

				if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;
				if (b->account->gc == zgc) {
					const char *chk;

					chk = local_zephyr_normalize(b->name);
					/* doesn't matter if this fails or not; we'll just move on to the next one */
					ZRequestLocations(chk, &ald, UNACKED, ZAUTH);
					free(ald.user);
					free(ald.version);
				}
			}
		}
	}

	return TRUE;
}

static char *get_exposure_level()
{
	char *exposure = ZGetVariable("exposure");

	if (!exposure)
		return EXPOSE_REALMVIS;
	if (!g_ascii_strcasecmp(exposure, EXPOSE_NONE))
		return EXPOSE_NONE;
	if (!g_ascii_strcasecmp(exposure, EXPOSE_OPSTAFF))
		return EXPOSE_OPSTAFF;
	if (!g_ascii_strcasecmp(exposure, EXPOSE_REALMANN))
		return EXPOSE_REALMANN;
	if (!g_ascii_strcasecmp(exposure, EXPOSE_NETVIS))
		return EXPOSE_NETVIS;
	if (!g_ascii_strcasecmp(exposure, EXPOSE_NETANN))
		return EXPOSE_NETANN;
	return EXPOSE_REALMVIS;
}

static void strip_comments(char *str)
{
	char *tmp = strchr(str, '#');

	if (tmp)
		*tmp = '\0';
	g_strchug(str);
	g_strchomp(str);
}

static void process_zsubs()
{
	FILE *f;
	gchar *fname;
	gchar buff[BUFSIZ];

	fname = g_strdup_printf("%s/.zephyr.subs", gaim_home_dir());
	f = fopen(fname, "r");
	if (f) {
		char **triple;
		ZSubscription_t sub;
		char *recip;

		while (fgets(buff, BUFSIZ, f)) {
			strip_comments(buff);
			if (buff[0]) {
				triple = g_strsplit(buff, ",", 3);
				if (triple[0] && triple[1]) {
					char *tmp = g_strdup_printf("%s", gaim_zephyr_get_sender());
					char *atptr;

					sub.zsub_class = triple[0];
					sub.zsub_classinst = triple[1];
					if (triple[2] == NULL) {
						recip = g_malloc0(1);
					} else if (!g_ascii_strcasecmp(triple[2], "%me%")) {
						recip = g_strdup_printf("%s", gaim_zephyr_get_sender());
					} else if (!g_ascii_strcasecmp(triple[2], "*")) {
						/* wildcard
						 * form of class,instance,* */
						recip = g_malloc0(1);
					} else if (!g_ascii_strcasecmp(triple[2], tmp)) {
						/* form of class,instance,aatharuv@ATHENA.MIT.EDU */
						recip = g_strdup(triple[2]);
					} else if ((atptr = strchr(triple[2], '@')) != NULL) {
						/* form of class,instance,*@ANDREW.CMU.EDU
						 * class,instance,@ANDREW.CMU.EDU
						 * If realm is local realm, blank recipient, else
						 * @REALM-NAME
						 */
						char *realmat = g_strdup_printf("@%s",
														gaim_zephyr_get_realm());

						if (!g_ascii_strcasecmp(atptr, realmat))
							recip = g_malloc0(1);
						else
							recip = g_strdup(atptr);
						g_free(realmat);
					} else {
						recip = g_strdup(triple[2]);
					}
					g_free(tmp);
					sub.zsub_recipient = recip;
					if (ZSubscribeTo(&sub, 1, 0) != ZERR_NONE) {
						gaim_debug(GAIM_DEBUG_ERROR, "zephyr", "Couldn't subscribe to %s, %s, %s\n", sub.zsub_class, sub.zsub_classinst, sub.zsub_recipient);
					}
					subscrips = g_slist_append(subscrips, new_triple(triple[0], triple[1], recip));
					g_free(recip);
				}
				g_strfreev(triple);
			}
		}
	}
}

static void process_anyone()
{
	FILE *fd;
	gchar buff[BUFSIZ], *filename;
	GaimGroup *g;
	GaimBuddy *b;

	if (!(g = gaim_find_group(_("Anyone")))) {
		g = gaim_group_new(_("Anyone"));
		gaim_blist_add_group(g, NULL);
	}

	filename = g_strconcat(gaim_home_dir(), "/.anyone", NULL);
	if ((fd = fopen(filename, "r")) != NULL) {
		while (fgets(buff, BUFSIZ, fd)) {
			strip_comments(buff);
			if (buff[0]) {
				if (!(b = gaim_find_buddy(zgc->account, buff))) {
					b = gaim_buddy_new(zgc->account, buff, NULL);
					gaim_blist_add_buddy(b, NULL, g, NULL);
				}
			}
		}
		fclose(fd);
	}
	g_free(filename);
}

static void zephyr_login(GaimAccount * account)
{
	ZSubscription_t sub;
	unsigned short port = 0;
	FILE* wgfile;
	char* wgfilename;
	if (zgc) {
		gaim_notify_error(account->gc, NULL,
						  _("Already logged in with Zephyr"), _("Because Zephyr uses your system username, you " "are unable to have multiple accounts on it " "when logged in as the same user."));
		return;
	}

	zgc = gaim_account_get_connection(account);
	zgc->flags |= GAIM_CONNECTION_HTML;
	gaim_connection_update_progress(zgc, _("Connecting"), 0, 2);

	z_call_s(ZInitialize(), "Couldn't initialize zephyr");
	z_call_s(ZOpenPort(&port), "Couldn't open port");
	z_call_s(ZSetLocation((char *)
						  gaim_account_get_string(zgc->account, "exposure_level", EXPOSE_REALMVIS)), "Couldn't set location");

	sub.zsub_class = "MESSAGE";
	sub.zsub_classinst = "PERSONAL";
	sub.zsub_recipient = (char *)gaim_zephyr_get_sender();
	
	/* we don't care if this fails. i'm lying right now. */
	if (ZSubscribeTo(&sub, 1, 0) != ZERR_NONE) {
		gaim_debug(GAIM_DEBUG_ERROR, "zephyr", "Couldn't subscribe to messages!\n");
	}

	wgfile = gaim_mkstemp_template(&wgfilename,"gaimwgXXXXXX");
	if (wgfile) {
		fprintf(wgfile,"%d\n",port);
		fclose(wgfile);
	}
	gaim_connection_set_state(zgc, GAIM_CONNECTED);
	serv_finish_login(zgc);

	process_anyone();
	process_zsubs();

	nottimer = gaim_timeout_add(100, check_notify, NULL);
	loctimer = gaim_timeout_add(20000, check_loc, NULL);
}

static void write_zsubs()
{
	GSList *s = subscrips;
	zephyr_triple *zt;
	FILE *fd;
	char *fname;

	char **triple;

	fname = g_strdup_printf("%s/.zephyr.subs", gaim_home_dir());
	fd = fopen(fname, "w");

	if (!fd) {
		g_free(fname);
		return;
	}

	while (s) {
		zt = s->data;
		triple = g_strsplit(zt->name, ",", 3);
		if (triple[2] != NULL) {
			if (!g_ascii_strcasecmp(triple[2], "")) {
				fprintf(fd, "%s,%s,*\n", triple[0], triple[1]);
			} else if (!g_ascii_strcasecmp(triple[2], gaim_zephyr_get_sender())) {
				fprintf(fd, "%s,%s,%%me%%\n", triple[0], triple[1]);
			} else {
				fprintf(fd, "%s\n", zt->name);
			}
		} else {
			fprintf(fd, "%s,%s,*\n", triple[0], triple[1]);
		}
		g_free(triple);
		s = s->next;
	}
	g_free(fname);
	fclose(fd);
}

static void write_anyone()
{
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimBuddy *b;
	char *ptr, *fname, *ptr2;
	FILE *fd;

	fname = g_strdup_printf("%s/.anyone", gaim_home_dir());
	fd = fopen(fname, "w");
	if (!fd) {
		g_free(fname);
		return;
	}

	for (gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for (cnode = gnode->child; cnode; cnode = cnode->next) {
			if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;
			for (bnode = cnode->child; bnode; bnode = bnode->next) {
				if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;
				b = (GaimBuddy *) bnode;
				if (b->account == zgc->account) {
					if ((ptr = strchr(b->name, '@')) != NULL) {
						ptr2 = ptr + 1;
						/* We should only strip the realm name if the principal
						   is in the user's realm
						 */
						if (!g_ascii_strcasecmp(ptr2, gaim_zephyr_get_realm())) {
							*ptr = '\0';
						}
					}
					fprintf(fd, "%s\n", b->name);
					if (ptr)
						*ptr = '@';
				}
			}
		}
	}

	fclose(fd);
	g_free(fname);
}

static void zephyr_close(GaimConnection * gc)
{
	GList *l;
	GSList *s;

	l = pending_zloc_names;
	while (l) {
		g_free((char *)l->data);
		l = l->next;
	}
	g_list_free(pending_zloc_names);

	if (gaim_account_get_bool(zgc->account, "write_anyone", FALSE))
		write_anyone();

	if (gaim_account_get_bool(zgc->account, "write_zsubs", FALSE))
		write_zsubs();

	s = subscrips;
	while (s) {
		free_triple((zephyr_triple *) s->data);
		s = s->next;
	}
	g_slist_free(subscrips);

	if (nottimer)
		gaim_timeout_remove(nottimer);
	nottimer = 0;
	if (loctimer)
		gaim_timeout_remove(loctimer);
	loctimer = 0;
	zgc = NULL;
	z_call(ZCancelSubscriptions(0));
	z_call(ZUnsetLocation());
	z_call(ZClosePort());
}

static int zephyr_chat_send(GaimConnection * gc, int id, const char *im)
{
	ZNotice_t notice;
	zephyr_triple *zt;
	char *buf;
	const char *sig;
	GaimConversation *gconv1;
	GaimConvChat *gcc;
	char *inst;
	char *html_buf;
	char *html_buf2;

	zt = find_sub_by_id(id);
	if (!zt)
		/* this should never happen. */
		return -EINVAL;

	sig = ZGetVariable("zwrite-signature");
	if (!sig) {
		sig = g_get_real_name();
	}

	html_buf = html_to_zephyr(im);
	html_buf2 = gaim_unescape_html(html_buf);

	buf = g_strdup_printf("%s%c%s", sig, '\0', html_buf2);

	gconv1 = gaim_find_conversation_with_account(zt->name, zgc->account);
	gcc = gaim_conversation_get_chat_data(gconv1);
	
	/* Woops. Setting instance to "PERSONAL" which is the default
	   sending instance if no instance is set */

	if (!(inst = (char *)gaim_conv_chat_get_topic(gcc)))
		inst = "PERSONAL";

	bzero((char *)&notice, sizeof(notice));
	notice.z_kind = ACKED;
	notice.z_port = 0;
	notice.z_opcode = "";
	notice.z_class = zt->class;
	notice.z_class_inst = inst;
	if (!g_ascii_strcasecmp(zt->recipient, "*"))
		notice.z_recipient = local_zephyr_normalize("");
	else
		notice.z_recipient = local_zephyr_normalize(zt->recipient);
	notice.z_sender = 0;
	notice.z_default_format = "Class $class, Instance $instance:\n" "To: @bold($recipient) at $time $date\n" "From: @bold($1) <$sender>\n\n$2";
	notice.z_message_len = strlen(html_buf2) + strlen(sig) + 2;
	notice.z_message = buf;
	g_free(html_buf);
	g_free(html_buf2);

	ZSendNotice(&notice, ZAUTH);
	g_free(buf);
	return 0;
}

static int zephyr_send_im(GaimConnection * gc, const char *who, const char *im, GaimConvImFlags flags)
{
	ZNotice_t notice;
	char *buf;
	const char *sig;
	char *html_buf;
	char *html_buf2;

	if (flags & GAIM_CONV_IM_AUTO_RESP)
		sig = "Automated reply:";
	else {
		sig = ZGetVariable("zwrite-signature");
		if (!sig) {
			sig = g_get_real_name();
		}
	}

	html_buf = html_to_zephyr(im);
	html_buf2 = gaim_unescape_html(html_buf);

	buf = g_strdup_printf("%s%c%s", sig, '\0', html_buf2);
	bzero((char *)&notice, sizeof(notice));
	notice.z_kind = ACKED;
	notice.z_port = 0;
	notice.z_opcode = "";
	notice.z_class = "MESSAGE";
	notice.z_class_inst = "PERSONAL";
	notice.z_sender = 0;
	notice.z_recipient = local_zephyr_normalize(who);
	notice.z_default_format = "Class $class, Instance $instance:\n" "To: @bold($recipient) at $time $date\n" "From: @bold($1) <$sender>\n\n$2";
	notice.z_message_len = strlen(html_buf2) + strlen(sig) + 2;
	notice.z_message = buf;
	g_free(html_buf2);
	g_free(html_buf);

	ZSendNotice(&notice, ZAUTH);
	g_free(buf);
	return 1;
}

static const char *zephyr_normalize(const GaimAccount * account, const char *orig)
{
	/* returns the string you gave it. Maybe this function shouldn't be here */
	static char buf[80];

	if (!g_ascii_strcasecmp(orig, "")) {
		buf[0] = '\0';
		return buf;
	} else {
		g_snprintf(buf, 80, "%s", orig);
	}
	return buf;
}


char *local_zephyr_normalize(const char *orig)
{
	/* 
	   Basically the inverse of zephyr_strip_foreign_realm 
	   
	 */	
	static char buf[80];

	if (!g_ascii_strcasecmp(orig, "")) {
		buf[0] = '\0';
		return buf;
	}

	if (strchr(orig, '@')) {
		g_snprintf(buf, 80, "%s", orig);
	} else {
		g_snprintf(buf, 80, "%s@%s", orig, gaim_zephyr_get_realm());
	}
	return buf;
}

static void zephyr_zloc(GaimConnection *gc, const char *who)
{
	ZAsyncLocateData_t ald;
	gchar* normalized_who = local_zephyr_normalize(who);

	if (ZRequestLocations(normalized_who, &ald, UNACKED, ZAUTH) == ZERR_NONE) {
		pending_zloc_names = g_list_append(pending_zloc_names,
				g_strdup(normalized_who));
	}
}

static void zephyr_set_away(GaimConnection * gc, const char *state, const char *msg)
{
	if (gc->away) {
		g_free(gc->away);
		gc->away = NULL;
	}

	if (!g_ascii_strcasecmp(state, _("Hidden"))) {
		ZSetLocation(EXPOSE_OPSTAFF);
		gc->away = g_strdup("");
	} else if (!g_ascii_strcasecmp(state, _("Online")))
		ZSetLocation(get_exposure_level());
	else /* state is GAIM_AWAY_CUSTOM */ if (msg)
		gc->away = g_strdup(msg);
}

static GList *zephyr_away_states(GaimConnection * gc)
{
	GList *m = NULL;

	m = g_list_append(m, _("Online"));
	m = g_list_append(m, GAIM_AWAY_CUSTOM);
	m = g_list_append(m, _("Hidden"));

	return m;
}

static GList *zephyr_chat_info(GaimConnection * gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);

	pce->label = _("_Class:");
	pce->identifier = "class";
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);

	pce->label = _("_Instance:");
	pce->identifier = "instance";
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);

	pce->label = _("_Recipient:");
	pce->identifier = "recipient";
	m = g_list_append(m, pce);

	return m;
}

static void zephyr_join_chat(GaimConnection * gc, GHashTable * data)
{
	ZSubscription_t sub;
	zephyr_triple *zt1, *zt2;
	const char *classname;
	const char *instname;
	const char *recip;
	GaimConversation *gconv;
	GaimConvChat *gcc;

	classname = g_hash_table_lookup(data, "class");
	instname = g_hash_table_lookup(data, "instance");
	recip = g_hash_table_lookup(data, "recipient");

	if (!classname)
		return;
	if (!instname || !strlen(instname))
		instname = "*";
	if (!recip || (*recip == '*'))
		recip = "";
	if (!g_ascii_strcasecmp(recip, "%me%"))
		recip = gaim_zephyr_get_sender();

	zt1 = new_triple(classname, instname, recip);
	zt2 = find_sub_by_triple(zt1);
	if (zt2) {
		free_triple(zt1);
		if (!zt2->open) {
			serv_got_joined_chat(gc, zt2->id, zt2->name);
			gconv = gaim_find_conversation_with_account(zt2->name, zgc->account);
			gcc = gaim_conversation_get_chat_data(gconv);
			if (!g_ascii_strcasecmp(instname,"*")) 
				instname = "PERSONAL";
			gaim_conv_chat_set_topic(gcc, gaim_zephyr_get_sender(), instname);
			zt2->open = TRUE;
		}	
		return;
	}

	sub.zsub_class = zt1->class;
	sub.zsub_classinst = zt1->instance;
	sub.zsub_recipient = zt1->recipient;

	if (ZSubscribeTo(&sub, 1, 0) != ZERR_NONE) {
		free_triple(zt1);
		return;
	}

	subscrips = g_slist_append(subscrips, zt1);
	zt1->open = TRUE;
	serv_got_joined_chat(gc, zt1->id, zt1->name);
	gconv = gaim_find_conversation_with_account(zt1->name, zgc->account);
	gcc = gaim_conversation_get_chat_data(gconv);
	if (!g_ascii_strcasecmp(instname,"*")) 
		instname = "PERSONAL";
	gaim_conv_chat_set_topic(gcc, gaim_zephyr_get_sender(), instname);
}

static void zephyr_chat_leave(GaimConnection * gc, int id)
{
	zephyr_triple *zt;

	zt = find_sub_by_id(id);
	if (zt) {
		zt->open = FALSE;
		zt->id = ++last_id;
	}
}

static const char *zephyr_list_icon(GaimAccount * a, GaimBuddy * b)
{
	return "zephyr";
}


static void zephyr_chat_set_topic(GaimConnection * gc, int id, const char *topic)
{
	zephyr_triple *zt;
	GaimConversation *gconv;
	GaimConvChat *gcc;
	char *sender = (char *)gaim_zephyr_get_sender();

	zt = find_sub_by_id(id);
	gconv = gaim_find_conversation_with_account(zt->name, zgc->account);
	gcc = gaim_conversation_get_chat_data(gconv);
	gaim_conv_chat_set_topic(gcc, sender, topic);

}

static GaimPlugin *my_protocol = NULL;

static GaimPluginProtocolInfo prpl_info = {
	GAIM_PRPL_API_VERSION,
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_NO_PASSWORD,
	NULL,
	NULL,
        NO_BUDDY_ICONS,
        zephyr_list_icon,
	NULL,
	NULL,
	NULL,
	zephyr_away_states,
	NULL,
	zephyr_chat_info,
	zephyr_login,
	zephyr_close,
	zephyr_send_im,
	NULL,
	NULL,
	zephyr_zloc,
	zephyr_set_away,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	zephyr_join_chat,
	NULL,						/* reject chat invite */
	NULL,
	zephyr_chat_leave,
	NULL,
	zephyr_chat_send,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	zephyr_normalize,
	NULL,
	NULL,
	NULL,
	zephyr_chat_set_topic,
	NULL,
	NULL,
	NULL,
	NULL
};

static GaimPluginInfo info = {
	GAIM_PLUGIN_API_VERSION,			  /**< api_version    */
	GAIM_PLUGIN_PROTOCOL,				  /**< type           */
	NULL,						  /**< ui_requirement */
	0,							  /**< flags          */
	NULL,						  /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,				  /**< priority       */

	"prpl-zephyr",					   /**< id             */
	"Zephyr",						 /**< name           */
	VERSION,						  /**< version        */
							  /**  summary        */
	N_("Zephyr Protocol Plugin"),
							  /**  description    */
	N_("Zephyr Protocol Plugin"),
	NULL,						  /**< author         */
	GAIM_WEBSITE,					  /**< homepage       */

	NULL,						  /**< load           */
	NULL,						  /**< unload         */
	NULL,						  /**< destroy        */

	NULL,						  /**< ui_info        */
	&prpl_info,					  /**< extra_info     */
	NULL,
	NULL
};

static void init_plugin(GaimPlugin * plugin)
{
	GaimAccountOption *option;
	char *tmp = get_exposure_level();

	option = gaim_account_option_bool_new(_("Export to .anyone"), "write_anyone", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_bool_new(_("Export to .zephyr.subs"), "write_zsubs", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Exposure"), "exposure_level", tmp ? tmp : EXPOSE_REALMVIS);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Encoding"), "encoding", ZEPHYR_FALLBACK_CHARSET);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(zephyr, init_plugin, info);
