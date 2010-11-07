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
#include "util.h"
#include "version.h"
#include "yahoo.h"
#include "yahoo_aliases.h"
#include "yahoo_friend.h"
#include "yahoo_packet.h"

/* I hate hardcoding this stuff, but Yahoo never sends us anything to use.  Someone in the know may be able to tweak this URL */
#define YAHOO_ALIAS_FETCH_URL "http://address.yahoo.com/yab/us?v=XM&prog=ymsgr&.intl=us&diffs=1&t=0&tags=short&rt=0&prog-ver=8.1.0.249&useutf8=1&legenc=codepage-1252"
#define YAHOO_ALIAS_UPDATE_URL "http://address.yahoo.com/yab/us?v=XM&prog=ymsgr&.intl=us&sync=1&tags=short&noclear=1&useutf8=1&legenc=codepage-1252"
#define YAHOOJP_ALIAS_FETCH_URL "http://address.yahoo.co.jp/yab/jp?v=XM&prog=ymsgr&.intl=jp&diffs=1&t=0&tags=short&rt=0&prog-ver=7.0.0.7"
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


/**************************************************************************
 * Alias Fetch Functions
 **************************************************************************/

static void
yahoo_fetch_aliases_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, size_t len, const gchar *error_message)
{
	struct callback_data *cb = user_data;
	PurpleConnection *gc = cb->gc;
	struct yahoo_data *yd = gc->proto_data;

	yd->url_datas = g_slist_remove(yd->url_datas, url_data);

	if (len == 0) {
		purple_debug_info("yahoo", "No Aliases to process.%s%s\n",
						  error_message ? " Error:" : "", error_message ? error_message : "");
	} else {
		gchar *full_name, *nick_name;
		const char *yid, *id, *fn, *ln, *nn, *alias;
		YahooFriend *f;
		PurpleBuddy *b;
		xmlnode *item, *contacts;

		/* Put our web response into a xmlnode for easy management */
		contacts = xmlnode_from_str(url_text, -1);

		if (contacts == NULL) {
			purple_debug_error("yahoo", "Badly formed Alias XML\n");
			g_free(cb->who);
			g_free(cb->id);
			g_free(cb);
			return;
		}
		purple_debug_info("yahoo", "Fetched %" G_GSIZE_FORMAT
				" bytes of alias data\n", len);

		/* Loop around and around and around until we have gone through all the received aliases  */
		for(item = xmlnode_get_child(contacts, "ct"); item; item = xmlnode_get_next_twin(item)) {
			/* Yahoo replies with two types of contact (ct) record, we are only interested in the alias ones */
			if ((yid = xmlnode_get_attrib(item, "yi"))) {
				/* Grab all the bits of information we can */
				fn = xmlnode_get_attrib(item, "fn");
				ln = xmlnode_get_attrib(item, "ln");
				nn = xmlnode_get_attrib(item, "nn");
				id = xmlnode_get_attrib(item, "id");

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
				f = yahoo_friend_find(cb->gc, yid);
				b = purple_find_buddy(cb->gc->account, yid);

				/*  If we don't find a matching buddy, ignore the alias !!  */
				if (f != NULL && b != NULL) {
					const char *buddy_alias = purple_buddy_get_alias(b);
					yahoo_friend_set_alias_id(f, id);

					/* Finally, if we received an alias, we better update the buddy list */
					if (alias != NULL) {
						serv_got_alias(cb->gc, yid, alias);
						purple_debug_info("yahoo", "Fetched alias '%s' (%s)\n", alias, id);
					} else if (buddy_alias != NULL && strcmp(buddy_alias, "") != 0) {
					/* Or if we have an alias that Yahoo doesn't, send it up */
						yahoo_update_alias(cb->gc, yid, buddy_alias);
						purple_debug_info("yahoo", "Sent updated alias '%s'\n", buddy_alias);
					}
				}

				g_free(full_name);
				g_free(nick_name);
			}
		}
		xmlnode_free(contacts);
	}

	g_free(cb->who);
	g_free(cb->id);
	g_free(cb);
}

void
yahoo_fetch_aliases(PurpleConnection *gc)
{
	struct yahoo_data *yd = gc->proto_data;
	struct callback_data *cb;
	const char *url;
	gchar *request, *webpage, *webaddress;
	PurpleUtilFetchUrlData *url_data;

	gboolean use_whole_url = FALSE;

	/* use whole URL if using HTTP Proxy */
	if ((gc->account->proxy_info)
	    && (gc->account->proxy_info->type == PURPLE_PROXY_HTTP))
		use_whole_url = TRUE;

	/* Using callback_data so I have access to gc in the callback function */
	cb = g_new0(struct callback_data, 1);
	cb->gc = gc;

	/*  Build all the info to make the web request */
	url = yd->jp ? YAHOOJP_ALIAS_FETCH_URL : YAHOO_ALIAS_FETCH_URL;
	purple_url_parse(url, &webaddress, NULL, &webpage, NULL, NULL);
	request = g_strdup_printf("GET %s%s/%s HTTP/1.1\r\n"
				 "User-Agent: Mozilla/4.0 (compatible; MSIE 5.5)\r\n"
				 "Cookie: T=%s; Y=%s\r\n"
				 "Host: %s\r\n"
				 "Cache-Control: no-cache\r\n\r\n",
				  use_whole_url ? "http://" : "", use_whole_url ? webaddress : "", webpage,
				  yd->cookie_t, yd->cookie_y,
				  webaddress);

	/* We have a URL and some header information, let's connect and get some aliases  */
	url_data = purple_util_fetch_url_request(url, use_whole_url, NULL, TRUE,
						 request, FALSE,
						 yahoo_fetch_aliases_cb, cb);
	if (url_data != NULL)
		yd->url_datas = g_slist_prepend(yd->url_datas, url_data);

	g_free(webaddress);
	g_free(webpage);
	g_free(request);
}

/**************************************************************************
 * Alias Update Functions
 **************************************************************************/

static void
yahoo_update_alias_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, size_t len, const gchar *error_message)
{
	xmlnode *node, *result;
	struct callback_data *cb = user_data;
	PurpleConnection *gc = cb->gc;
	struct yahoo_data *yd;

	yd = gc->proto_data;
	yd->url_datas = g_slist_remove(yd->url_datas, url_data);

	if (len == 0 || error_message != NULL) {
		purple_debug_info("yahoo", "Error updating alias for %s: %s\n",
				  cb->who,
				  error_message ? error_message : "");
		g_free(cb->who);
		g_free(cb->id);
		g_free(cb);
		return;
	}

	result = xmlnode_from_str(url_text, -1);

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
	struct yahoo_data *yd;
	const char *url;
	gchar *content, *request, *webpage, *webaddress;
	struct callback_data *cb;
	PurpleUtilFetchUrlData *url_data;
	gboolean use_whole_url = FALSE;
	YahooFriend *f;

	/* use whole URL if using HTTP Proxy */
	if ((gc->account->proxy_info)
	    && (gc->account->proxy_info->type == PURPLE_PROXY_HTTP))
		use_whole_url = TRUE;

	g_return_if_fail(who != NULL);
	g_return_if_fail(gc != NULL);

	if (alias == NULL)
		alias = "";

	f = yahoo_friend_find(gc, who);
	if (f == NULL) {
		purple_debug_error("yahoo", "Missing YahooFriend. Unable to set server alias.\n");
		return;
	}

	yd = gc->proto_data;

	/* Using callback_data so I have access to gc in the callback function */
	cb = g_new0(struct callback_data, 1);
	cb->who = g_strdup(who);
	cb->id = g_strdup(yahoo_friend_get_alias_id(f));
	cb->gc = gc;

	/*  Build all the info to make the web request */
	url = yd->jp ? YAHOOJP_ALIAS_UPDATE_URL: YAHOO_ALIAS_UPDATE_URL;
	purple_url_parse(url, &webaddress, NULL, &webpage, NULL, NULL);

	if (cb->id == NULL) {
		/* No id for this buddy, so create an address book entry */
		purple_debug_info("yahoo", "Creating '%s' as new alias for user '%s'\n", alias, who);

		if (yd->jp) {
			gchar *alias_jp = g_convert(alias, -1, "EUC-JP", "UTF-8", NULL, NULL, NULL);
			gchar *converted_alias_jp = yahoo_convert_to_numeric(alias_jp);
			content = g_strdup_printf("<ab k=\"%s\" cc=\"9\">\n"
						  "<ct a=\"1\" yi='%s' nn='%s' />\n</ab>\r\n",
						  purple_account_get_username(gc->account),
						  who, converted_alias_jp);
			free(converted_alias_jp);
			g_free(alias_jp);
		} else {
			gchar *escaped_alias = g_markup_escape_text(alias, -1);
			content = g_strdup_printf("<?xml version=\"1.0\" encoding=\"utf-8\"?><ab k=\"%s\" cc=\"9\">\n"
						  "<ct a=\"1\" yi='%s' nn='%s' />\n</ab>\r\n",
						  purple_account_get_username(gc->account),
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
						  purple_account_get_username(gc->account),
						  who, cb->id, converted_alias_jp);
			free(converted_alias_jp);
			g_free(alias_jp);
		} else {
			gchar *escaped_alias = g_markup_escape_text(alias, -1);
			content = g_strdup_printf("<?xml version=\"1.0\" encoding=\"utf-8\"?><ab k=\"%s\" cc=\"1\">\n"
						  "<ct e=\"1\"  yi='%s' id='%s' nn='%s' pr='0' />\n</ab>\r\n",
						  purple_account_get_username(gc->account),
						  who, cb->id, escaped_alias);
			g_free(escaped_alias);
		}
	}

	request = g_strdup_printf("POST %s%s/%s HTTP/1.1\r\n"
				  "User-Agent: Mozilla/4.0 (compatible; MSIE 5.5)\r\n"
				  "Cookie: T=%s; Y=%s\r\n"
				  "Host: %s\r\n"
				  "Content-Length: %" G_GSIZE_FORMAT "\r\n"
				  "Cache-Control: no-cache\r\n\r\n"
				  "%s",
				  use_whole_url ? "http://" : "", use_whole_url ? webaddress : "", webpage,
				  yd->cookie_t, yd->cookie_y,
				  webaddress,
				  strlen(content),
				  content);

	/* We have a URL and some header information, let's connect and update the alias  */
	url_data = purple_util_fetch_url_request(url, use_whole_url, NULL, TRUE, request, FALSE, yahoo_update_alias_cb, cb);
	if (url_data != NULL)
		yd->url_datas = g_slist_prepend(yd->url_datas, url_data);

	g_free(webpage);
	g_free(webaddress);
	g_free(content);
	g_free(request);
}

