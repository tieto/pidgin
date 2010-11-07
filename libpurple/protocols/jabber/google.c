/**
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"
#include "debug.h"
#include "util.h"
#include "privacy.h"

#include "buddy.h"
#include "google.h"
#include "jabber.h"
#include "presence.h"
#include "iq.h"

static void
jabber_gmail_parse(JabberStream *js, xmlnode *packet, gpointer nul)
{
	const char *type = xmlnode_get_attrib(packet, "type");
	xmlnode *child;
	xmlnode *message, *sender_node, *subject_node;
	const char *from, *to, *url, *tid;
	char *subject;
	const char *in_str;
	char *to_name;
	char *default_tos[1];

	int i, count = 1, returned_count;

	const char **tos, **froms, **urls;
	char **subjects;

	if (strcmp(type, "result"))
		return;

	child = xmlnode_get_child(packet, "mailbox");
	if (!child)
		return;

	in_str = xmlnode_get_attrib(child, "total-matched");
	if (in_str && *in_str)
		count = atoi(in_str);

	/* If Gmail doesn't tell us who the mail is to, let's use our JID */
	to = xmlnode_get_attrib(packet, "to");
	default_tos[0] = jabber_get_bare_jid(to);

	message = xmlnode_get_child(child, "mail-thread-info");

	if (count == 0 || !message) {
		if (count > 0)
			purple_notify_emails(js->gc, count, FALSE, NULL, NULL, (const char**) default_tos, NULL, NULL, NULL);
		g_free(default_tos[0]);
		return;
	}

	/* Loop once to see how many messages were returned so we can allocate arrays
	 * accordingly */
	for (returned_count = 0; message; returned_count++, message=xmlnode_get_next_twin(message));

	froms    = g_new0(const char* , returned_count);
	tos      = g_new0(const char* , returned_count);
	subjects = g_new0(char* , returned_count);
	urls     = g_new0(const char* , returned_count);

	to = xmlnode_get_attrib(packet, "to");
	to_name = jabber_get_bare_jid(to);
	url = xmlnode_get_attrib(child, "url");
	if (!url || !*url)
		url = "http://www.gmail.com";

	message= xmlnode_get_child(child, "mail-thread-info");
	for (i=0; message; message = xmlnode_get_next_twin(message), i++) {
		subject_node = xmlnode_get_child(message, "subject");
		sender_node  = xmlnode_get_child(message, "senders");
		sender_node  = xmlnode_get_child(sender_node, "sender");

		while (sender_node && (!xmlnode_get_attrib(sender_node, "unread") ||
		       !strcmp(xmlnode_get_attrib(sender_node, "unread"),"0")))
			sender_node = xmlnode_get_next_twin(sender_node);

		if (!sender_node) {
			i--;
			continue;
		}

		from = xmlnode_get_attrib(sender_node, "name");
		if (!from || !*from)
			from = xmlnode_get_attrib(sender_node, "address");
		subject = xmlnode_get_data(subject_node);
		/*
		 * url = xmlnode_get_attrib(message, "url");
		 */
		tos[i] = (to_name != NULL ?  to_name : "");
		froms[i] = (from != NULL ?  from : "");
		subjects[i] = (subject != NULL ? subject : g_strdup(""));
		urls[i] = url;

		tid = xmlnode_get_attrib(message, "tid");
		if (tid &&
		    (js->gmail_last_tid == NULL || strcmp(tid, js->gmail_last_tid) > 0)) {
			g_free(js->gmail_last_tid);
			js->gmail_last_tid = g_strdup(tid);
		}
	}

	if (i>0)
		purple_notify_emails(js->gc, count, count == i, (const char**) subjects, froms, tos,
				urls, NULL, NULL);
	else
		purple_notify_emails(js->gc, count, FALSE, NULL, NULL, (const char**) default_tos, NULL, NULL, NULL);


	g_free(to_name);
	g_free(tos);
	g_free(default_tos[0]);
	g_free(froms);
	for (; i > 0; i--)
		g_free(subjects[i - 1]);
	g_free(subjects);
	g_free(urls);

	in_str = xmlnode_get_attrib(child, "result-time");
	if (in_str && *in_str) {
		g_free(js->gmail_last_time);
		js->gmail_last_time = g_strdup(in_str);
	}
}

void
jabber_gmail_poke(JabberStream *js, xmlnode *packet)
{
	const char *type;
	xmlnode *query;
	JabberIq *iq;

	/* bail if the user isn't interested */
	if (!purple_account_get_check_mail(js->gc->account))
		return;

	type = xmlnode_get_attrib(packet, "type");


	/* Is this an initial incoming mail notification? If so, send a request for more info */
	if (strcmp(type, "set") || !xmlnode_get_child(packet, "new-mail"))
		return;

	purple_debug(PURPLE_DEBUG_MISC, "jabber",
		   "Got new mail notification. Sending request for more info\n");

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "google:mail:notify");
	jabber_iq_set_callback(iq, jabber_gmail_parse, NULL);
	query = xmlnode_get_child(iq->node, "query");

	if (js->gmail_last_time)
		xmlnode_set_attrib(query, "newer-than-time", js->gmail_last_time);
	if (js->gmail_last_tid)
		xmlnode_set_attrib(query, "newer-than-tid", js->gmail_last_tid);

	jabber_iq_send(iq);
	return;
}

void jabber_gmail_init(JabberStream *js) {
	JabberIq *iq;

	if (!purple_account_get_check_mail(js->gc->account))
		return;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "google:mail:notify");
	jabber_iq_set_callback(iq, jabber_gmail_parse, NULL);
	jabber_iq_send(iq);
}

void jabber_google_roster_init(JabberStream *js)
{
	JabberIq *iq;
	xmlnode *query;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:roster");
	query = xmlnode_get_child(iq->node, "query");

	xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
	xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);
}

void jabber_google_roster_outgoing(JabberStream *js, xmlnode *query, xmlnode *item)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	GSList *list = account->deny;
	const char *jid = xmlnode_get_attrib(item, "jid");
	char *jid_norm = g_strdup(jabber_normalize(account, jid));

	while (list) {
		if (!strcmp(jid_norm, (char*)list->data)) {
			xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
			xmlnode_set_attrib(item, "gr:t", "B");
			xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
			xmlnode_set_attrib(query, "gr:ext", "2");
			return;
		}
		list = list->next;
	}

	g_free(jid_norm);

}

gboolean jabber_google_roster_incoming(JabberStream *js, xmlnode *item)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	GSList *list = account->deny;
	const char *jid = xmlnode_get_attrib(item, "jid");
	gboolean on_block_list = FALSE;

	char *jid_norm;

	const char *grt = xmlnode_get_attrib_with_namespace(item, "t", "google:roster");
	const char *subscription = xmlnode_get_attrib(item, "subscription");

	if (!subscription || !strcmp(subscription, "none")) {
		/* The Google Talk servers will automatically add people from your Gmail address book
		 * with subscription=none. If we see someone with subscription=none, ignore them.
		 */
		return FALSE;
	}

 	jid_norm = g_strdup(jabber_normalize(account, jid));

	while (list) {
		if (!strcmp(jid_norm, (char*)list->data)) {
			on_block_list = TRUE;
			break;
		}
		list = list->next;
	}

	if (grt && (*grt == 'H' || *grt == 'h')) {
		PurpleBuddy *buddy = purple_find_buddy(account, jid_norm);
		if (buddy)
			purple_blist_remove_buddy(buddy);
		g_free(jid_norm);
		return FALSE;
	}

	if (!on_block_list && (grt && (*grt == 'B' || *grt == 'b'))) {
		purple_debug_info("jabber", "Blocking %s\n", jid_norm);
		purple_privacy_deny_add(account, jid_norm, TRUE);
	} else if (on_block_list && (!grt || (*grt != 'B' && *grt != 'b' ))){
		purple_debug_info("jabber", "Unblocking %s\n", jid_norm);
		purple_privacy_deny_remove(account, jid_norm, TRUE);
	}

	g_free(jid_norm);
	return TRUE;
}

void jabber_google_roster_add_deny(PurpleConnection *gc, const char *who)
{
	JabberStream *js;
	GSList *buddies;
	JabberIq *iq;
	xmlnode *query;
	xmlnode *item;
	xmlnode *group;
	PurpleBuddy *b;
	JabberBuddy *jb;

	js = (JabberStream*)(gc->proto_data);

	if (!js || !js->server_caps & JABBER_CAP_GOOGLE_ROSTER)
		return;

	jb = jabber_buddy_find(js, who, TRUE);

	buddies = purple_find_buddies(js->gc->account, who);
	if(!buddies)
		return;

	b = buddies->data;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	while(buddies) {
		PurpleGroup *g;

		b = buddies->data;
		g = purple_buddy_get_group(b);

		group = xmlnode_new_child(item, "group");
		xmlnode_insert_data(group, g->name, -1);

		buddies = buddies->next;
	}

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	xmlnode_set_attrib(item, "jid", who);
	xmlnode_set_attrib(item, "name", b->alias ? b->alias : "");
	xmlnode_set_attrib(item, "gr:t", "B");
	xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
	xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);

	/* Synthesize a sign-off */
	if (jb) {
		JabberBuddyResource *jbr;
		GList *l = jb->resources;
		while (l) {
			jbr = l->data;
			if (jbr && jbr->name)
			{
				purple_debug(PURPLE_DEBUG_MISC, "jabber", "Removing resource %s\n", jbr->name);
				jabber_buddy_remove_resource(jb, jbr->name);
			}
			l = l->next;
		}
	}
	purple_prpl_got_user_status(purple_connection_get_account(gc), who, "offline", NULL);
}

void jabber_google_roster_rem_deny(PurpleConnection *gc, const char *who)
{
	JabberStream *js;
	GSList *buddies;
	JabberIq *iq;
	xmlnode *query;
	xmlnode *item;
	xmlnode *group;
	PurpleBuddy *b;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(who != NULL);

	js = (JabberStream*)(gc->proto_data);

	if (!js || !js->server_caps & JABBER_CAP_GOOGLE_ROSTER)
		return;

	buddies = purple_find_buddies(js->gc->account, who);
	if(!buddies)
		return;

	b = buddies->data;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	while(buddies) {
		PurpleGroup *g;

		b = buddies->data;
		g = purple_buddy_get_group(b);

		group = xmlnode_new_child(item, "group");
		xmlnode_insert_data(group, g->name, -1);

		buddies = buddies->next;
	}

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	xmlnode_set_attrib(item, "jid", who);
	xmlnode_set_attrib(item, "name", b->alias ? b->alias : "");
	xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
	xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);

	/* See if he's online */
	jabber_presence_subscription_set(js, who, "probe");
}

/* This does two passes on the string. The first pass goes through
 * and determine if all the structured text is properly balanced, and
 * how many instances of each there is. The second pass goes and converts
 * everything to HTML, depending on what's figured out by the first pass.
 * It will short circuit once it knows it has no more replacements to make
 */
char *jabber_google_format_to_html(const char *text)
{
	const char *p;

	/* The start of the screen may be consdiered a space for this purpose */
	gboolean preceding_space = TRUE;

	gboolean in_bold = FALSE, in_italic = FALSE;
	gboolean in_tag = FALSE;

	gint bold_count = 0, italic_count = 0;

	GString *str;

	for (p = text; *p != '\0'; p = g_utf8_next_char(p)) {
		gunichar c = g_utf8_get_char(p);
		if (c == '*' && !in_tag) {
			if (in_bold && (g_unichar_isspace(*(p+1)) ||
					*(p+1) == '\0' ||
					*(p+1) == '<')) {
				bold_count++;
				in_bold = FALSE;
			} else if (preceding_space && !in_bold && !g_unichar_isspace(*(p+1))) {
				bold_count++;
				in_bold = TRUE;
			}
			preceding_space = TRUE;
		} else if (c == '_' && !in_tag) {
			if (in_italic && (g_unichar_isspace(*(p+1)) ||
					*(p+1) == '\0' ||
					*(p+1) == '<')) {
				italic_count++;
				in_italic = FALSE;
			} else if (preceding_space && !in_italic && !g_unichar_isspace(*(p+1))) {
				italic_count++;
				in_italic = TRUE;
			}
			preceding_space = TRUE;
		} else if (c == '<' && !in_tag) {
			in_tag = TRUE;
		} else if (c == '>' && in_tag) {
			in_tag = FALSE;
		} else if (!in_tag) {
			if (g_unichar_isspace(c))
				preceding_space = TRUE;
			else
				preceding_space = FALSE;
		}
	}

	str  = g_string_new(NULL);
	in_bold = in_italic = in_tag = FALSE;
	preceding_space = TRUE;

	for (p = text; *p != '\0'; p = g_utf8_next_char(p)) {
		gunichar c = g_utf8_get_char(p);

		if (bold_count < 2 && italic_count < 2 && !in_bold && !in_italic) {
			g_string_append(str, p);
			return g_string_free(str, FALSE);
		}


		if (c == '*' && !in_tag) {
			if (in_bold &&
			    (g_unichar_isspace(*(p+1))||*(p+1)=='<')) { /* This is safe in UTF-8 */
				str = g_string_append(str, "</b>");
				in_bold = FALSE;
				bold_count--;
			} else if (preceding_space && bold_count > 1 && !g_unichar_isspace(*(p+1))) {
				str = g_string_append(str, "<b>");
				bold_count--;
				in_bold = TRUE;
			} else {
				str = g_string_append_unichar(str, c);
			}
			preceding_space = TRUE;
		} else if (c == '_' && !in_tag) {
			if (in_italic &&
			    (g_unichar_isspace(*(p+1))||*(p+1)=='<')) {
				str = g_string_append(str, "</i>");
				italic_count--;
				in_italic = FALSE;
			} else if (preceding_space && italic_count > 1 && !g_unichar_isspace(*(p+1))) {
				str = g_string_append(str, "<i>");
				italic_count--;
				in_italic = TRUE;
			} else {
				str = g_string_append_unichar(str, c);
			}
			preceding_space = TRUE;
		} else if (c == '<' && !in_tag) {
			str = g_string_append_unichar(str, c);
			in_tag = TRUE;
		} else if (c == '>' && in_tag) {
			str = g_string_append_unichar(str, c);
			in_tag = FALSE;
		} else if (!in_tag) {
			str = g_string_append_unichar(str, c);
			if (g_unichar_isspace(c))
				preceding_space = TRUE;
			else
				preceding_space = FALSE;
		} else {
			str = g_string_append_unichar(str, c);
		}
	}
	return g_string_free(str, FALSE);
}

void jabber_google_presence_incoming(JabberStream *js, const char *user, JabberBuddyResource *jbr)
{
	if (!js->googletalk)
		return;
	if (jbr->status && !strncmp(jbr->status, "♫ ", strlen("♫ "))) {
		purple_prpl_got_user_status(js->gc->account, user, "tune",
					    PURPLE_TUNE_TITLE, jbr->status + strlen("♫ "), NULL);
		jbr->status = NULL;
	} else {
		purple_prpl_got_user_status_deactive(js->gc->account, user, "tune");
	}
}

char *jabber_google_presence_outgoing(PurpleStatus *tune)
{
	const char *attr = purple_status_get_attr_string(tune, PURPLE_TUNE_TITLE);
	return attr ? g_strdup_printf("♫ %s", attr) : g_strdup("");
}
