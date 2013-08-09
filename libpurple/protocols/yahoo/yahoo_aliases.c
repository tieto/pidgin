/*
 * purple
 *
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
 *
 */


#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "blist.h"
#include "debug.h"
#include "http.h"
#include "util.h"
#include "request.h"
#include "version.h"
#include "libymsg.h"
#include "yahoo_aliases.h"
#include "yahoo_friend.h"
#include "yahoo_packet.h"

/* I hate hardcoding this stuff, but Yahoo never sends us anything to use.  Someone in the know may be able to tweak this URL */
#define YAHOO_ALIAS_FETCH_URL "http://address.yahoo.com/yab/us?v=XM&prog=ymsgr&.intl=us&diffs=1&t=0&tags=short&rt=0&prog-ver=" YAHOO_CLIENT_VERSION "&useutf8=1&legenc=codepage-1252"
#define YAHOO_ALIAS_UPDATE_URL "http://address.yahoo.com/yab/us?v=XM&prog=ymsgr&.intl=us&sync=1&tags=short&noclear=1&useutf8=1&legenc=codepage-1252"
#define YAHOOJP_ALIAS_FETCH_URL "http://address.yahoo.co.jp/yab/jp?v=XM&prog=ymsgr&.intl=jp&diffs=1&t=0&tags=short&rt=0&prog-ver=" YAHOOJP_CLIENT_VERSION
#define YAHOOJP_ALIAS_UPDATE_URL "http://address.yahoo.co.jp/yab/jp?v=XM&prog=ymsgr&.intl=jp&sync=1&tags=short&noclear=1"

void yahoo_update_alias(PurpleConnection *gc, const char *who, const char *alias);

/**
 * Stuff we want passed to the callback function
 */
struct callback_data {
	PurpleConnection *gc;
	gchar *id;
	gchar *who;
};

void yahoo_personal_details_reset(YahooPersonalDetails *ypd, gboolean all)
{
	if (all)
		g_free(ypd->id);
	g_free(ypd->names.first);
	g_free(ypd->names.last);
	g_free(ypd->names.middle);
	g_free(ypd->names.nick);
	g_free(ypd->phone.work);
	g_free(ypd->phone.home);
	g_free(ypd->phone.mobile);
}

/**************************************************************************
 * Alias Fetch Functions
 **************************************************************************/

static void
yahoo_fetch_aliases_cb(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _unused)
{
	PurpleConnection *gc =
		purple_http_conn_get_purple_connection(http_conn);
	YahooData *yd = purple_connection_get_protocol_data(gc);

	if (!purple_http_response_is_successful(response)) {
		purple_debug_info("yahoo", "yahoo_fetch_aliases_cb error: %s\n",
			purple_http_response_get_error(response));
	} else {
		gchar *full_name, *nick_name;
		const gchar *xml_raw;
		const char *yid, *id, *fn, *ln, *nn, *alias, *mn;
		const char *hp, *wp, *mo;
		YahooFriend *f;
		PurpleBuddy *b;
		xmlnode *item, *contacts;
		PurpleAccount *account;
		size_t len;

		account = purple_connection_get_account(gc);
		/* Put our web response into a xmlnode for easy management */
		xml_raw = purple_http_response_get_data(response, &len);
		contacts = xmlnode_from_str(xml_raw, -1);

		if (purple_debug_is_verbose()) {
			purple_debug_misc("yahoo",
				"yahoo_fetch_aliases_cb xml:[%s]\n", xml_raw);
		}

		if (contacts == NULL) {
			purple_debug_error("yahoo", "Badly formed Alias XML\n");
			return;
		}
		purple_debug_info("yahoo", "Fetched %" G_GSIZE_FORMAT
				" bytes of alias data\n", len);

		/* Loop around and around and around until we have gone through all the received aliases  */
		for(item = xmlnode_get_child(contacts, "ct"); item; item = xmlnode_get_next_twin(item)) {
			/* Yahoo replies with two types of contact (ct) record, we are only interested in the alias ones */
			if ((yid = xmlnode_get_attrib(item, "yi"))) {
				YahooPersonalDetails *ypd = NULL;
				/* Grab all the bits of information we can */
				fn = xmlnode_get_attrib(item, "fn");
				ln = xmlnode_get_attrib(item, "ln");
				nn = xmlnode_get_attrib(item, "nn");
				mn = xmlnode_get_attrib(item, "mn");
				id = xmlnode_get_attrib(item, "id");

				hp = xmlnode_get_attrib(item, "hp");
				wp = xmlnode_get_attrib(item, "wp");
				mo = xmlnode_get_attrib(item, "mo");

				full_name = nick_name = NULL;
				alias = NULL;

				/* Yahoo stores first and last names separately, lets put them together into a full name */
				if (yd->jp)
					full_name = g_strstrip(g_strdup_printf("%s %s", (ln != NULL ? ln : "") , (fn != NULL ? fn : "")));
				else
					full_name = g_strstrip(g_strdup_printf("%s %s", (fn != NULL ? fn : "") , (ln != NULL ? ln : "")));
				nick_name = (nn != NULL ? g_strstrip(g_strdup(nn)) : NULL);

				if (nick_name != NULL)
					alias = nick_name;   /* If we have a nickname from Yahoo, let's use it */
				else if (strlen(full_name) != 0)
					alias = full_name;  /* If no Yahoo nickname, we can use the full_name created above */

				/*  Find the local buddy that matches */
				f = yahoo_friend_find(gc, yid);
				b = purple_find_buddy(account, yid);

				/*  If we don't find a matching buddy, ignore the alias !!  */
				if (f != NULL && b != NULL) {
					const char *buddy_alias = purple_buddy_get_alias(b);
					yahoo_friend_set_alias_id(f, id);

					/* Finally, if we received an alias, we better update the buddy list */
					if (alias != NULL) {
						serv_got_alias(gc, yid, alias);
						purple_debug_info("yahoo", "Fetched alias '%s' (%s)\n", alias, id);
					} else if (buddy_alias && *buddy_alias && !g_str_equal(buddy_alias, yid)) {
					/* Or if we have an alias that Yahoo doesn't, send it up */
						yahoo_update_alias(gc, yid, buddy_alias);
						purple_debug_info("yahoo", "Sent updated alias '%s'\n", buddy_alias);
					}
				}

				if (f != NULL)
					ypd = &f->ypd;
				else {
					/* May be the alias is for the account? */
					const char *yidn = purple_normalize(account, yid);
					if (purple_strequal(yidn, purple_connection_get_display_name(gc))) {
						ypd = &yd->ypd;
					}
				}

				if (ypd) {
					yahoo_personal_details_reset(ypd, TRUE);
					ypd->id = g_strdup(id);
					ypd->names.first = g_strdup(fn);
					ypd->names.middle = g_strdup(mn);
					ypd->names.last = g_strdup(ln);
					ypd->names.nick = g_strdup(nn);

					ypd->phone.work = g_strdup(wp);
					ypd->phone.home = g_strdup(hp);
					ypd->phone.mobile = g_strdup(mo);
				}

				g_free(full_name);
				g_free(nick_name);
			}
		}
		xmlnode_free(contacts);
	}
}

void
yahoo_fetch_aliases(PurpleConnection *gc)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleHttpRequest *req;
	PurpleHttpCookieJar *cookiejar;

	req = purple_http_request_new(yd->jp ? YAHOOJP_ALIAS_FETCH_URL :
		YAHOO_ALIAS_FETCH_URL);
	/* XXX: see the other note about user-agent */
	purple_http_request_header_set(req, "User-Agent",
		"Mozilla/4.0 (compatible; MSIE 5.5)");
	cookiejar = purple_http_request_get_cookie_jar(req);
	purple_http_cookie_jar_set(cookiejar, "T", yd->cookie_t);
	purple_http_cookie_jar_set(cookiejar, "Y", yd->cookie_y);
	purple_http_connection_set_add(yd->http_reqs, purple_http_request(gc,
		req, yahoo_fetch_aliases_cb, NULL));
	purple_http_request_unref(req);
}

/**************************************************************************
 * Alias Update Functions
 **************************************************************************/

static void
yahoo_update_alias_cb(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _cb)
{
	xmlnode *node, *result;
	struct callback_data *cb = _cb;

	if (!purple_http_response_is_successful(response)) {
		purple_debug_info("yahoo", "Error updating alias for %s: %s\n",
			cb->who, purple_http_response_get_error(response));
		g_free(cb->who);
		g_free(cb->id);
		g_free(cb);
		return;
	}

	result = xmlnode_from_str(
		purple_http_response_get_data(response, NULL), -1);

	if (result == NULL) {
		purple_debug_error("yahoo", "Alias update for %s failed: Badly formed response\n",
				   cb->who);
		g_free(cb->who);
		g_free(cb->id);
		g_free(cb);
		return;
	}

	if ((node = xmlnode_get_child(result, "ct"))) {
		if (cb->id == NULL) {
			const char *new_id = xmlnode_get_attrib(node, "id");
			if (new_id != NULL) {
				/* We now have an addressbook id for the friend; we should save it */
				YahooFriend *f = yahoo_friend_find(cb->gc, cb->who);

				purple_debug_info("yahoo", "Alias creation for %s succeeded\n", cb->who);

				if (f)
					yahoo_friend_set_alias_id(f, new_id);
				else
					purple_debug_error("yahoo", "Missing YahooFriend. Unable to store new addressbook id.\n");
			} else
				purple_debug_error("yahoo", "Missing new addressbook id in add response for %s (weird).\n",
						   cb->who);
		} else {
			if (g_ascii_strncasecmp(xmlnode_get_attrib(node, "id"), cb->id, strlen(cb->id))==0)
				purple_debug_info("yahoo", "Alias update for %s succeeded\n", cb->who);
			else
				purple_debug_error("yahoo", "Alias update for %s failed (Contact record return mismatch)\n",
						   cb->who);
		}
	} else
		purple_debug_info("yahoo", "Alias update for %s failed (No contact record returned)\n", cb->who);

	g_free(cb->who);
	g_free(cb->id);
	g_free(cb);
	xmlnode_free(result);
}

void
yahoo_update_alias(PurpleConnection *gc, const char *who, const char *alias)
{
	PurpleHttpRequest *req;
	PurpleHttpCookieJar *cookiejar;
	YahooData *yd;
	gchar *content;
	struct callback_data *cb;
	YahooFriend *f;

	g_return_if_fail(who != NULL);
	g_return_if_fail(gc != NULL);

	if (alias == NULL)
		alias = "";

	f = yahoo_friend_find(gc, who);
	if (f == NULL) {
		purple_debug_error("yahoo", "Missing YahooFriend. Unable to set server alias.\n");
		return;
	}

	yd = purple_connection_get_protocol_data(gc);

	/* Using callback_data so I have access to gc in the callback function */
	cb = g_new0(struct callback_data, 1);
	cb->who = g_strdup(who);
	cb->id = g_strdup(yahoo_friend_get_alias_id(f));
	cb->gc = gc;

	if (cb->id == NULL) {
		/* No id for this buddy, so create an address book entry */
		purple_debug_info("yahoo", "Creating '%s' as new alias for user '%s'\n", alias, who);

		if (yd->jp) {
			gchar *alias_jp = g_convert(alias, -1, "EUC-JP", "UTF-8", NULL, NULL, NULL);
			gchar *converted_alias_jp = yahoo_convert_to_numeric(alias_jp);
			content = g_strdup_printf("<ab k=\"%s\" cc=\"9\">\n"
						  "<ct a=\"1\" yi='%s' nn='%s' />\n</ab>\r\n",
						  purple_account_get_username(purple_connection_get_account(gc)),
						  who, converted_alias_jp);
			g_free(converted_alias_jp);
			g_free(alias_jp);
		} else {
			gchar *escaped_alias = g_markup_escape_text(alias, -1);
			content = g_strdup_printf("<?xml version=\"1.0\" encoding=\"utf-8\"?><ab k=\"%s\" cc=\"9\">\n"
						  "<ct a=\"1\" yi='%s' nn='%s' />\n</ab>\r\n",
						  purple_account_get_username(purple_connection_get_account(gc)),
						  who, escaped_alias);
			g_free(escaped_alias);
		}
	} else {
		purple_debug_info("yahoo", "Updating '%s' as new alias for user '%s'\n", alias, who);

		if (yd->jp) {
			gchar *alias_jp = g_convert(alias, -1, "EUC-JP", "UTF-8", NULL, NULL, NULL);
			gchar *converted_alias_jp = yahoo_convert_to_numeric(alias_jp);
			content = g_strdup_printf("<ab k=\"%s\" cc=\"1\">\n"
						  "<ct e=\"1\"  yi='%s' id='%s' nn='%s' pr='0' />\n</ab>\r\n",
						  purple_account_get_username(purple_connection_get_account(gc)),
						  who, cb->id, converted_alias_jp);
			g_free(converted_alias_jp);
			g_free(alias_jp);
		} else {
			gchar *escaped_alias = g_markup_escape_text(alias, -1);
			content = g_strdup_printf("<?xml version=\"1.0\" encoding=\"utf-8\"?><ab k=\"%s\" cc=\"1\">\n"
						  "<ct e=\"1\"  yi='%s' id='%s' nn='%s' pr='0' />\n</ab>\r\n",
						  purple_account_get_username(purple_connection_get_account(gc)),
						  who, cb->id, escaped_alias);
			g_free(escaped_alias);
		}
	}

	req = purple_http_request_new(yd->jp ? YAHOOJP_ALIAS_UPDATE_URL:
		YAHOO_ALIAS_UPDATE_URL);
	purple_http_request_set_method(req, "POST");
	/* XXX: We get rs="ERROR:-100:No Login", when we set
	 * YAHOO_CLIENT_USERAGENT (Mozilla/5.0) here.
	 */
	purple_http_request_header_set(req, "User-Agent",
		"Mozilla/4.0 (compatible; MSIE 5.5)");
	cookiejar = purple_http_request_get_cookie_jar(req);
	purple_http_cookie_jar_set(cookiejar, "T", yd->cookie_t);
	purple_http_cookie_jar_set(cookiejar, "Y", yd->cookie_y);
	purple_http_request_set_contents(req, content, -1);
	purple_http_connection_set_add(yd->http_reqs, purple_http_request(gc,
		req, yahoo_update_alias_cb, cb));
	purple_http_request_unref(req);

	g_free(content);
}


/**************************************************************************
 * User Info Update Functions
 **************************************************************************/

#if 0
/* This block of code can be used to send our contact details to
 * everyone in the buddylist. But with the official messenger,
 * doing this pops a conversation window at the receiver's end,
 * which is stupid, and thus not really surprising. */

struct yahoo_userinfo {
	YahooData *yd;
	char *xml;
};

static void
yahoo_send_userinfo_to_user(struct yahoo_userinfo *yui, const char *who)
{
	struct yahoo_packet *pkt;
	PurpleConnection *gc;

	gc = yui->yd->gc;
	pkt = yahoo_packet_new(YAHOO_SERVICE_CONTACT_DETAILS, 0, 0);
	yahoo_packet_hash(pkt, "siisis",
			1, purple_connection_get_display_name(gc),
			13, 1,    /* This creates a conversation window in the official client */
			302, 5,
			5, who,
			303, 5,
			280, yui->xml);
	yahoo_packet_send_and_free(pkt, yui->yd);
}

static void
yahoo_send_userinfo_foreach(gpointer key, gpointer value, gpointer data)
{
	const char *who = key;
	YahooFriend *f = value;

	if (f->status != YAHOO_STATUS_OFFLINE) {
		yahoo_send_userinfo_to_user(data, who);
	}
}

static void
yahoo_sent_userinfo_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, size_t len, const gchar *error_message)
{
	struct yahoo_userinfo *yui = user_data;
	yahoo_fetch_aliases_cb(url_data, yui->yd->gc, url_text, len, error_message);
	g_hash_table_foreach(yui->yd->friends, yahoo_send_userinfo_foreach, yui);
	g_free(yui->xml);
	g_free(yui);
}
#endif

static void
yahoo_set_userinfo_cb(PurpleConnection *gc, PurpleRequestFields *fields)
{
	PurpleHttpRequest *req;
	PurpleHttpCookieJar *cookiejar;

	xmlnode *node = xmlnode_new("ab");
	xmlnode *ct = xmlnode_new_child(node, "ct");
	YahooData *yd = purple_connection_get_protocol_data(gc);
	char *content;
	int len;
	int i;
	char * yfields[] = { "fn", "ln", "nn", "mn", "hp", "wp", "mo", NULL };

	xmlnode_set_attrib(node, "k", purple_connection_get_display_name(gc));
	xmlnode_set_attrib(node, "cc", "1");		/* XXX: ? */

	xmlnode_set_attrib(ct, "e", "1");
	xmlnode_set_attrib(ct, "yi", purple_request_fields_get_string(fields, "yname"));
	xmlnode_set_attrib(ct, "id", purple_request_fields_get_string(fields, "yid"));
	xmlnode_set_attrib(ct, "pr", "0");

	for (i = 0; yfields[i]; i++) {
		const char *v = purple_request_fields_get_string(fields, yfields[i]);
		xmlnode_set_attrib(ct, yfields[i], v ? v : "");
	}

	content = xmlnode_to_formatted_str(node, &len);
	xmlnode_free(node);

#if 0
	{
		/* This is if we wanted to send our contact details to everyone
		 * in the buddylist. But this cannot be done now, because in the
		 * official messenger, doing this pops a conversation window at
		 * the receiver's end, which is stupid, and thus not really
		 * surprising. */
		struct yahoo_userinfo *ui = g_new(struct yahoo_userinfo, 1);
		node = xmlnode_new("contact");

		for (i = 0; yfields[i]; i++) {
			const char *v = purple_request_fields_get_string(fields, yfields[i]);
			if (v) {
				xmlnode *nd = xmlnode_new_child(node, yfields[i]);
				xmlnode_insert_data(nd, v, -1);
			}
		}

		ui->yd = yd;
		ui->xml = xmlnode_to_str(node, NULL);
		xmlnode_free(node);
	}
#endif

	req = purple_http_request_new(yd->jp ? YAHOOJP_USERINFO_URL :
		YAHOO_USERINFO_URL);
	purple_http_request_set_method(req, "POST");
	/* XXX: see the previous comment for user-agent */
	purple_http_request_header_set(req, "User-Agent",
		"Mozilla/4.0 (compatible; MSIE 5.5)");
	cookiejar = purple_http_request_get_cookie_jar(req);
	purple_http_cookie_jar_set(cookiejar, "T", yd->cookie_t);
	purple_http_cookie_jar_set(cookiejar, "Y", yd->cookie_y);
	purple_http_request_set_contents(req, content, -1);
	purple_http_connection_set_add(yd->http_reqs, purple_http_request(gc,
		req, yahoo_fetch_aliases_cb, NULL));
	purple_http_request_unref(req);

	g_free(content);
}

static PurpleRequestFields *
request_fields_from_personal_details(YahooPersonalDetails *ypd, const char *id)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	int i;
	struct {
		char *id;
		char *text;
		char *value;
	} yfields[] = {
		{"fn", N_("First Name"), ypd->names.first},
		{"ln", N_("Last Name"), ypd->names.last},
		{"nn", N_("Nickname"), ypd->names.nick},
		{"mn", N_("Middle Name"), ypd->names.middle},
		{"hp", N_("Home Phone Number"), ypd->phone.home},
		{"wp", N_("Work Phone Number"), ypd->phone.work},
		{"mo", N_("Mobile Phone Number"), ypd->phone.mobile},
		{NULL, NULL, NULL}
	};

	if (ypd->id == NULL) {
		purple_debug_error("yahoo",
			"request_fields_from_personal_details:"
			"ypd->id == NULL (it doesn't seems to work)\n");
		return NULL;
	}

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("yname", "", id, FALSE);
	purple_request_field_set_visible(field, FALSE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("yid", "", ypd->id, FALSE);
	purple_request_field_set_visible(field, FALSE);
	purple_request_field_group_add_field(group, field);

	for (i = 0; yfields[i].id; i++) {
		field = purple_request_field_string_new(yfields[i].id, _(yfields[i].text),
				yfields[i].value, FALSE);
		purple_request_field_group_add_field(group, field);
	}

	return fields;
}

void yahoo_set_userinfo_for_buddy(PurpleConnection *gc, PurpleBuddy *buddy)
{
	PurpleRequestFields *fields;
	YahooFriend *f;
	const char *name;

	name = purple_buddy_get_name(buddy);
	f = yahoo_friend_find(gc, name);
	if (!f)
		return;

	fields = request_fields_from_personal_details(&f->ypd, name);
	if (fields == NULL)
		return;
	purple_request_fields(gc, NULL, _("Set User Info"), NULL, fields,
			_("OK"), G_CALLBACK(yahoo_set_userinfo_cb),
			_("Cancel"), NULL,
			purple_connection_get_account(gc), NULL, NULL, gc);
}

void yahoo_set_userinfo(PurpleConnection *gc)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleRequestFields *fields = request_fields_from_personal_details(&yd->ypd,
					purple_connection_get_display_name(gc));
	if (fields == NULL)
		return;
	purple_request_fields(gc, NULL, _("Set User Info"), NULL, fields,
			_("OK"), G_CALLBACK(yahoo_set_userinfo_cb),
			_("Cancel"), NULL,
			purple_connection_get_account(gc), NULL, NULL, gc);
}

static gboolean
parse_contact_details(YahooData *yd, const char *who, const char *xml)
{
	xmlnode *node, *nd;
	YahooFriend *f;
	char *yid;

	node = xmlnode_from_str(xml, -1);
	if (!node) {
		purple_debug_info("yahoo", "Received malformed XML for contact details from '%s':\n%s\n",
				who, xml);
		return FALSE;
	}

	nd = xmlnode_get_child(node, "yi");
	if (!nd || !(yid = xmlnode_get_data(nd))) {
		xmlnode_free(node);
		return FALSE;
	}

	if (!purple_strequal(yid, who)) {
		/* The user may not want to set the contact details about folks in the buddylist
		   to what some random dude might have sent. So it would be good if we popped
		   up a prompt requiring the user to confirm the details before we set them.
		   However, someone could send details about hundreds of users at the same time,
		   which would make things really bad. So for now, until we have a better way of
		   dealing with this, ignore this details. */
		purple_debug_info("yahoo", "Ignoring contact details sent by %s about %s\n",
				who, yid);
		g_free(yid);
		xmlnode_free(node);
		return FALSE;
	}

	f = yahoo_friend_find(yd->gc, yid);
	if (!f) {
		g_free(yid);
		xmlnode_free(node);
		return FALSE;
	} else {
		int i;
		YahooPersonalDetails *ypd = &f->ypd;
		char *alias = NULL;
		struct {
			char *id;
			char **field;
		} details[] = {
			{"fn", &ypd->names.first},
			{"mn", &ypd->names.middle},
			{"ln", &ypd->names.last},
			{"nn", &ypd->names.nick},
			{"wp", &ypd->phone.work},
			{"hp", &ypd->phone.home},
			{"mo", &ypd->phone.mobile},
			{NULL, NULL}
		};

		yahoo_personal_details_reset(ypd, FALSE);

		for (i = 0; details[i].id; i++) {
			nd = xmlnode_get_child(node, details[i].id);
			*details[i].field = nd ? xmlnode_get_data(nd) : NULL;
		}

		if (ypd->names.nick)
			alias = ypd->names.nick;
		else if (ypd->names.first || ypd->names.last) {
			alias = g_strstrip(g_strdup_printf("%s %s",
						ypd->names.first ? ypd->names.first : "",
						ypd->names.last ? ypd->names.last : ""));
		}

		if (alias) {
			serv_got_alias(yd->gc, yid, alias);
			if (alias != ypd->names.nick)
				g_free(alias);
		}
	}

	xmlnode_free(node);
	g_free(yid);
	return TRUE;
}

/* I don't think this happens for MSN buddies. -- sad */
void yahoo_process_contact_details(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	const char *who = NULL, *xml = NULL;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	for (; l; l = l->next) {
		struct yahoo_pair *pair = l->data;
		switch (pair->key) {
			case 4:
				who = pair->value;	/* This is the person who sent us the details.
									   But not necessarily about himself. */
				break;
			case 5:
				break;
			case 13:
				/* This is '1' if 'who' is sending the contact details about herself,
				   '0' if 'who' is sending the contact details she has about buddies
				   in her list. However, in all cases, the xml in key 280 always seems
				   to contain the yid of the person, so we may as well ignore this field
				   and look into the xml instead to see who the information is about. */
				break;
			case 280:
				xml = pair->value;
				parse_contact_details(yd, who, xml);
				break;
		}
	}
}

