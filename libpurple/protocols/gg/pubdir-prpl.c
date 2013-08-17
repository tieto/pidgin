/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#include "pubdir-prpl.h"

#include <debug.h>
#include <http.h>
#include <request.h>

#include "oauth/oauth-purple.h"
#include "xml.h"
#include "utils.h"
#include "status.h"

typedef struct
{
	PurpleConnection *gc;
	ggp_pubdir_request_cb cb;
	void *user_data;
	enum
	{
		GGP_PUBDIR_REQUEST_TYPE_INFO,
		GGP_PUBDIR_REQUEST_TYPE_SEARCH,
	} type;
	union
	{
		struct
		{
			uin_t uin;
		} user_info;
		ggp_pubdir_search_form *search_form;
	} params;
} ggp_pubdir_request;

void ggp_pubdir_request_free(ggp_pubdir_request *request);
void ggp_pubdir_record_free(ggp_pubdir_record *records, int count);

static void ggp_pubdir_get_info_got_token(PurpleConnection *gc,
	const gchar *token, gpointer _request);
static void ggp_pubdir_got_data(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer user_data);

static void ggp_pubdir_get_info_protocol_got(PurpleConnection *gc,
	int records_count, const ggp_pubdir_record *records, int next_offset,
	void *_uin_p);

static void ggp_pubdir_request_buddy_alias_got(PurpleConnection *gc,
	int records_count, const ggp_pubdir_record *records, int next_offset,
	void *user_data);

// Searching for buddies.

#define GGP_PUBDIR_SEARCH_TITLE _("Gadu-Gadu Public Directory")
#define GGP_PUBDIR_SEARCH_PER_PAGE 20

struct _ggp_pubdir_search_form
{
	gchar *nick, *city;
	ggp_pubdir_gender gender;
	int offset;
	int limit;

	void *display_handle;
};

void ggp_pubdir_search_form_free(ggp_pubdir_search_form *form);
ggp_pubdir_search_form * ggp_pubdir_search_form_clone(
	const ggp_pubdir_search_form *form);

static void ggp_pubdir_search_request(PurpleConnection *gc,
	PurpleRequestFields *fields);
static gchar * ggp_pubdir_search_make_query(const ggp_pubdir_search_form *form);
static void ggp_pubdir_search_execute(PurpleConnection *gc,
	const ggp_pubdir_search_form *form,
	ggp_pubdir_request_cb cb, void *user_data);
static void ggp_pubdir_search_got_token(PurpleConnection *gc,
	const gchar *token, gpointer _request);
static void ggp_pubdir_search_results_display(PurpleConnection *gc,
	int records_count, const ggp_pubdir_record *records, int next_offset,
	void *user_data);
static void ggp_pubdir_search_results_close(gpointer _form);
static void ggp_pubdir_search_results_next(PurpleConnection *gc, GList *row,
	gpointer _form);
static void ggp_pubdir_search_results_add(PurpleConnection *gc, GList *row,
	gpointer _form);
static void ggp_pubdir_search_results_im(PurpleConnection *gc, GList *row,
	gpointer _form);
static void ggp_pubdir_search_results_info(PurpleConnection *gc, GList *row,
	gpointer _form);
static void ggp_pubdir_search_results_new(PurpleConnection *gc, GList *row,
	gpointer _form);

// Own profile.

static void ggp_pubdir_set_info_dialog(PurpleConnection *gc, int records_count,
	const ggp_pubdir_record *records, int next_offset, void *user_data);
static void ggp_pubdir_set_info_request(PurpleConnection *gc,
	PurpleRequestFields *fields);
static void ggp_pubdir_set_info_got_token(PurpleConnection *gc,
	const gchar *token, gpointer _record);

static void ggp_pubdir_set_info_got_response(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer user_data);

/******************************************************************************/

static const gchar *ggp_pubdir_provinces[] =
{
	"dolnośląskie",
	"kujawsko-pomorskie",
	"lubelskie",
	"lubuskie",
	"łódzkie",
	"małopolskie",
	"mazowieckie",
	"opolskie",
	"podkarpackie",
	"podlaskie",
	"pomorskie",
	"śląskie",
	"świętokrzyskie",
	"warmińsko-mazurskie",
	"wielkopolskie",
	"zachodniopomorskie",
};

static int ggp_pubdir_provinces_count = sizeof(ggp_pubdir_provinces)/sizeof(gchar*);

/******************************************************************************/

void ggp_pubdir_record_free(ggp_pubdir_record *records, int count)
{
	int i;
	for (i = 0; i < count; i++)
	{
		g_free(records[i].label);
		g_free(records[i].nickname);
		g_free(records[i].first_name);
		g_free(records[i].last_name);
		g_free(records[i].city);
	}
	g_free(records);
}

void ggp_pubdir_request_free(ggp_pubdir_request *request)
{
	if (request->type == GGP_PUBDIR_REQUEST_TYPE_SEARCH)
		ggp_pubdir_search_form_free(request->params.search_form);
	g_free(request);
}

void ggp_pubdir_get_info(PurpleConnection *gc, uin_t uin,
	ggp_pubdir_request_cb cb, void *user_data)
{
	ggp_pubdir_request *request = g_new0(ggp_pubdir_request, 1);
	gchar *url;

	request->type = GGP_PUBDIR_REQUEST_TYPE_INFO;
	request->gc = gc;
	request->cb = cb;
	request->user_data = user_data;
	request->params.user_info.uin = uin;

	url = g_strdup_printf("http://api.gadu-gadu.pl/users/%u", uin);
	ggp_oauth_request(gc, ggp_pubdir_get_info_got_token, request,
		"GET", url);
	g_free(url);
}

static void ggp_pubdir_get_info_got_token(PurpleConnection *gc,
	const gchar *token, gpointer _request)
{
	PurpleHttpRequest *req;
	ggp_pubdir_request *request = _request;

	if (!token || !PURPLE_CONNECTION_IS_VALID(gc))
	{
		request->cb(gc, -1, NULL, 0, request->user_data);
		ggp_pubdir_request_free(request);
		return;
	}

	req = purple_http_request_new(NULL);
	purple_http_request_set_url_printf(req,
		"http://api.gadu-gadu.pl/users/%u",
		request->params.user_info.uin);
	purple_http_request_header_set(req, "Authorization", token);
	purple_http_request(gc, req, ggp_pubdir_got_data, request);
	purple_http_request_unref(req);
}

static void ggp_pubdir_got_data(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _request)
{
	ggp_pubdir_request *request = _request;
	PurpleConnection *gc = request->gc;
	gboolean succ = TRUE;
	xmlnode *xml;
	const gchar *xml_raw;
	unsigned int status, next_offset;
	int record_count, i;
	ggp_pubdir_record *records;

	xml_raw = purple_http_response_get_data(response, NULL);

	if (purple_debug_is_verbose() && purple_debug_is_unsafe()) {
		purple_debug_misc("gg", "ggp_pubdir_got_data: xml=[%s]\n",
			xml_raw);
	}

	xml = xmlnode_from_str(xml_raw, -1);
	if (xml == NULL)
	{
		purple_debug_error("gg", "ggp_pubdir_got_data: "
			"invalid xml\n");
		request->cb(gc, -1, NULL, 0, request->user_data);
		ggp_pubdir_request_free(request);
		return;
	}
	
	succ &= ggp_xml_get_uint(xml, "status", &status);
	if (!ggp_xml_get_uint(xml, "nextOffset", &next_offset))
		next_offset = 0;
	xml = xmlnode_get_child(xml, "users");
	if (!succ || status != 0 || !xml)
	{
		purple_debug_error("gg", "ggp_pubdir_got_data: "
			"invalid reply\n");
		request->cb(gc, -1, NULL, 0, request->user_data);
		ggp_pubdir_request_free(request);
		return;
	}
	
	record_count = ggp_xml_child_count(xml, "user");
	records = g_new0(ggp_pubdir_record, record_count);
	
	xml = xmlnode_get_child(xml, "user");
	i = 0;
	while (xml)
	{
		ggp_pubdir_record *record = &records[i++];
		gchar *city = NULL, *birth_s = NULL;
		unsigned int gender = 0;
		const gchar *uin_s;
		
		g_assert(i <= record_count);
		
		record->uin = ggp_str_to_uin(xmlnode_get_attrib(xml, "uin"));
		if (record->uin == 0)
			ggp_xml_get_uint(xml, "uin", &record->uin);
		if (record->uin == 0)
			purple_debug_error("gg", "ggp_pubdir_got_data:"
				" invalid uin\n");
		uin_s = ggp_uin_to_str(record->uin);
		
		ggp_xml_get_string(xml, "label", &record->label);
		ggp_xml_get_string(xml, "nick", &record->nickname);
		ggp_xml_get_string(xml, "name", &record->first_name);
		ggp_xml_get_string(xml, "surname", &record->last_name);
		ggp_xml_get_string(xml, "city", &city);
		ggp_xml_get_string(xml, "birth", &birth_s);
		ggp_xml_get_uint(xml, "gender", &gender);
		ggp_xml_get_uint(xml, "age", &record->age);
		ggp_xml_get_uint(xml, "province", &record->province);
		
		record->label = ggp_free_if_equal(record->label, uin_s);
		record->label = ggp_free_if_equal(record->label, "");
		record->nickname = ggp_free_if_equal(record->nickname, uin_s);
		record->nickname = ggp_free_if_equal(record->nickname, "");
		record->first_name = ggp_free_if_equal(record->first_name, "");
		record->last_name = ggp_free_if_equal(record->last_name, "");
		
		if (record->label) {}
		else if (record->nickname)
			record->label = g_strdup(record->nickname);
		else if (record->first_name && record->last_name)
			record->label = g_strdup_printf("%s %s",
				record->first_name, record->last_name);
		else if (record->first_name)
			record->label = g_strdup(record->first_name);
		else if (record->last_name)
			record->label = g_strdup(record->last_name);
		if (record->label)
			g_strstrip(record->label);
		if (record->nickname)
			g_strstrip(record->nickname);
		
		if (gender == 1)
			record->gender = GGP_PUBDIR_GENDER_FEMALE;
		else if (gender == 2)
			record->gender = GGP_PUBDIR_GENDER_MALE;
		else
			record->gender = GGP_PUBDIR_GENDER_UNSPECIFIED;
		
		if (city && city[0] != '\0')
			record->city = g_strdup(city);
		if (record->city)
			g_strstrip(record->city);
		if (!record->city)
		{
			g_free(record->city);
			record->city = NULL;
		}
		
		record->birth = ggp_date_from_iso8601(birth_s);
		//TODO: calculate age from birth
		
		//TODO: verbose
		purple_debug_misc("gg", "ggp_pubdir_got_data: [uin:%d] "
			"[label:%s] [nick:%s] [first name:%s] [last name:%s] "
			"[city:%s] [gender:%d] [age:%d] [birth:%lu]\n",
			record->uin, record->label, record->nickname,
			record->first_name, record->last_name, record->city,
			record->gender, record->age, record->birth);
		
		g_free(city);
		
		xml = xmlnode_get_next_twin(xml);
	}
	
	request->cb(gc, record_count, records, next_offset, request->user_data);
	
	ggp_pubdir_request_free(request);
	ggp_pubdir_record_free(records, record_count);
}

void ggp_pubdir_get_info_protocol(PurpleConnection *gc, const char *name)
{
	uin_t uin = ggp_str_to_uin(name);
	uin_t *uin_p = g_new0(uin_t, 1);

	*uin_p = uin;

	purple_debug_info("gg", "ggp_pubdir_get_info_protocol: %u\n", uin);

	ggp_pubdir_get_info(gc, uin, ggp_pubdir_get_info_protocol_got, uin_p);
}

static void ggp_pubdir_get_info_protocol_got(PurpleConnection *gc,
	int records_count, const ggp_pubdir_record *records, int next_offset,
	void *_uin_p)
{
	uin_t uin = *((uin_t*)_uin_p);
	PurpleNotifyUserInfo *info = purple_notify_user_info_new();
	const ggp_pubdir_record *record = &records[0];
	PurpleBuddy *buddy;
	
	g_free(_uin_p);
	
	if (records_count < 1)
	{
		purple_debug_error("gg", "ggp_pubdir_get_info_protocol_got: "
			"couldn't get info for %u\n", uin);
		purple_notify_user_info_add_pair_plaintext(info, NULL,
			_("Cannot get user information"));
		purple_notify_userinfo(gc, ggp_uin_to_str(uin), info,
			NULL, NULL);
		purple_notify_user_info_destroy(info);
		return;
	}
	
	purple_debug_info("gg", "ggp_pubdir_get_info_protocol_got: %u\n", uin);
	g_assert(uin == record->uin);
	g_assert(records_count == 1);
	
	buddy = purple_blist_find_buddy(purple_connection_get_account(gc),
		ggp_uin_to_str(uin));
	if (buddy)
	{
		const char *alias;
		PurpleStatus *status;
		gchar *status_message;
		
		alias = purple_buddy_get_alias_only(buddy);
		if (alias)
			purple_notify_user_info_add_pair_plaintext(info,
				_("Alias"), alias);
		
		status = purple_presence_get_active_status(
			purple_buddy_get_presence(buddy));
		ggp_status_from_purplestatus(status, &status_message);
		purple_notify_user_info_add_pair_plaintext(info, _("Status"),
			purple_status_get_name(status));
		if (status_message)
			purple_notify_user_info_add_pair_plaintext(info,
				_("Message"), status_message);
	}
	
	if (record->nickname)
		purple_notify_user_info_add_pair_plaintext(info,
			_("Nickname"), record->nickname);
	if (record->first_name)
		purple_notify_user_info_add_pair_plaintext(info,
			_("First name"), record->first_name);
	if (record->last_name)
		purple_notify_user_info_add_pair_plaintext(info,
			_("Last name"), record->last_name);
	if (record->gender != GGP_PUBDIR_GENDER_UNSPECIFIED)
		purple_notify_user_info_add_pair_plaintext(info, _("Gender"),
			record->gender == GGP_PUBDIR_GENDER_FEMALE ?
			_("Female") : _("Male"));
	if (record->city)
		purple_notify_user_info_add_pair_plaintext(info, _("City"),
			record->city);
	if (record->birth)
		purple_notify_user_info_add_pair_plaintext(info, _("Birthday"),
			ggp_date_strftime("%Y-%m-%d", record->birth));
	else if (record->age)
	{
		gchar *age_s = g_strdup_printf("%d", record->age);
		purple_notify_user_info_add_pair_plaintext(info, _("Age"),
			age_s);
		g_free(age_s);
	}
	
	purple_notify_userinfo(gc, ggp_uin_to_str(uin), info, NULL, NULL);
	purple_notify_user_info_destroy(info);
}

void ggp_pubdir_request_buddy_alias(PurpleConnection *gc, PurpleBuddy *buddy)
{
	uin_t uin = ggp_str_to_uin(purple_buddy_get_name(buddy));

	purple_debug_info("gg", "ggp_pubdir_request_buddy_alias: %u\n", uin);

	ggp_pubdir_get_info(gc, uin, ggp_pubdir_request_buddy_alias_got, NULL);
}

static void ggp_pubdir_request_buddy_alias_got(PurpleConnection *gc,
	int records_count, const ggp_pubdir_record *records, int next_offset,
	void *user_data)
{
	uin_t uin;
	const gchar *alias;
	
	if (records_count < 0)
	{
		purple_debug_error("gg", "ggp_pubdir_request_buddy_alias_got: "
			"couldn't get info for user\n");
		return;
	}
	uin = records[0].uin;
	
	alias = records[0].label;
	if (!alias)
	{
		purple_debug_info("gg", "ggp_pubdir_request_buddy_alias_got: "
			"public alias for %u is not available\n", uin);
		return;
	}

	purple_debug_info("gg", "ggp_pubdir_request_buddy_alias_got: "
		"public alias for %u is \"%s\"\n", uin, alias);
	
	serv_got_alias(gc, ggp_uin_to_str(uin), alias);
}

/*******************************************************************************
 * Searching for buddies.
 ******************************************************************************/

void ggp_pubdir_search_form_free(ggp_pubdir_search_form *form)
{
	g_free(form->nick);
	g_free(form->city);
	g_free(form);
}

ggp_pubdir_search_form * ggp_pubdir_search_form_clone(
	const ggp_pubdir_search_form *form)
{
	ggp_pubdir_search_form *dup = g_new(ggp_pubdir_search_form, 1);
	
	dup->nick = g_strdup(form->nick);
	dup->city = g_strdup(form->city);
	dup->gender = form->gender;
	dup->offset = form->offset;
	dup->limit = form->limit;

	dup->display_handle = form->display_handle;
	
	return dup;
}

void ggp_pubdir_search(PurpleConnection *gc,
	const ggp_pubdir_search_form *form)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	int default_gender;
	
	purple_debug_info("gg", "ggp_pubdir_search\n");
	
	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);
	
	field = purple_request_field_string_new("name", _("Name"),
		form ? form->nick : NULL, FALSE);
	purple_request_field_group_add_field(group, field);
	
	field = purple_request_field_string_new("city", _("City"),
		form ? form->city : NULL, FALSE);
	purple_request_field_group_add_field(group, field);
	
	default_gender = 0;
	if (form && form->gender == GGP_PUBDIR_GENDER_MALE)
		default_gender = 1;
	else if (form && form->gender == GGP_PUBDIR_GENDER_FEMALE)
		default_gender = 2;
	
	field = purple_request_field_choice_new("gender", _("Gender"),
		default_gender);
	purple_request_field_choice_add(field, _("Male or female"));
	purple_request_field_choice_add(field, _("Male"));
	purple_request_field_choice_add(field, _("Female"));
	purple_request_field_group_add_field(group, field);
	
	purple_request_fields(gc, _("Find buddies"), _("Find buddies"),
		_("Please, enter your search criteria below"), fields,
		_("OK"), G_CALLBACK(ggp_pubdir_search_request),
		_("Cancel"), NULL,
		purple_connection_get_account(gc), NULL, NULL, gc);
}

static void ggp_pubdir_search_request(PurpleConnection *gc,
	PurpleRequestFields *fields)
{
	ggp_pubdir_search_form *form = g_new0(ggp_pubdir_search_form, 1);
	int gender;

	purple_debug_info("gg", "ggp_pubdir_search_request\n");
	
	form->nick = g_strdup(purple_request_fields_get_string(fields, "name"));
	form->city = g_strdup(purple_request_fields_get_string(fields, "city"));
	gender = purple_request_fields_get_choice(fields, "gender");
	if (gender == 1)
		form->gender = GGP_PUBDIR_GENDER_MALE;
	else if (gender == 2)
		form->gender = GGP_PUBDIR_GENDER_FEMALE;
	
	form->offset = 0;
	form->limit = GGP_PUBDIR_SEARCH_PER_PAGE;
	
	ggp_pubdir_search_execute(gc, form, ggp_pubdir_search_results_display,
		form);
}

static gchar * ggp_pubdir_search_make_query(const ggp_pubdir_search_form *form)
{
	gchar *nick, *city, *gender;
	gchar *query;
	
	if (form->nick && form->nick[0] != '\0')
	{
		gchar *nick_e = g_uri_escape_string(form->nick, NULL, FALSE);
		nick = g_strdup_printf("&nick=%s", nick_e);
		g_free(nick_e);
	}
	else
		nick = g_strdup("");
	
	if (form->city && form->city[0] != '\0')
	{
		gchar *city_e = g_uri_escape_string(form->city, NULL, FALSE);
		city = g_strdup_printf("&city=%s", city_e);
		g_free(city_e);
	}
	else
		city = g_strdup("");
	
	if (form->gender != GGP_PUBDIR_GENDER_UNSPECIFIED)
		gender = g_strdup_printf("&gender=%d",
			form->gender == GGP_PUBDIR_GENDER_MALE ? 2 : 1);
	else
		gender = g_strdup("");
	
	query = g_strdup_printf("/users.xml?offset=%d&limit=%d%s%s%s",
		form->offset, form->limit, nick, city, gender);
	
	g_free(nick);
	g_free(city);
	g_free(gender);
	
	return query;
}

static void ggp_pubdir_search_execute(PurpleConnection *gc,
	const ggp_pubdir_search_form *form,
	ggp_pubdir_request_cb cb, void *user_data)
{
	ggp_pubdir_request *request = g_new0(ggp_pubdir_request, 1);
	gchar *url;
	ggp_pubdir_search_form *local_form = ggp_pubdir_search_form_clone(form);
	gchar *query;

	request->type = GGP_PUBDIR_REQUEST_TYPE_SEARCH;
	request->gc = gc;
	request->cb = cb;
	request->user_data = user_data;
	request->params.search_form = local_form;

	query = ggp_pubdir_search_make_query(form);
	purple_debug_misc("gg", "ggp_pubdir_search_execute: %s\n", query);
	url = g_strdup_printf("http://api.gadu-gadu.pl%s", query);
	ggp_oauth_request(gc, ggp_pubdir_search_got_token, request,
		"GET", url);
	g_free(query);
	g_free(url);
}

static void ggp_pubdir_search_got_token(PurpleConnection *gc,
	const gchar *token, gpointer _request)
{
	PurpleHttpRequest *req;
	ggp_pubdir_request *request = _request;
	gchar *query;

	if (!token || !PURPLE_CONNECTION_IS_VALID(gc))
	{
		request->cb(gc, -1, NULL, 0, request->user_data);
		ggp_pubdir_request_free(request);
		return;
	}

	purple_debug_misc("gg", "ggp_pubdir_search_got_token\n");

	query = ggp_pubdir_search_make_query(request->params.search_form);

	req = purple_http_request_new(NULL);
	purple_http_request_set_url_printf(req, "http://api.gadu-gadu.pl%s", query);
	purple_http_request_header_set(req, "Authorization", token);
	purple_http_request(gc, req, ggp_pubdir_got_data, request);
	purple_http_request_unref(req);

	g_free(query);
}


static void ggp_pubdir_search_results_display(PurpleConnection *gc,
	int records_count, const ggp_pubdir_record *records, int next_offset,
	void *_form)
{
	ggp_pubdir_search_form *form = _form;
	PurpleNotifySearchResults *results;
	int i;

	purple_debug_info("gg", "ggp_pubdir_search_results_display: "
		"got %d records (next offset: %d)\n",
		records_count, next_offset);

	if (records_count < 0 ||
		(records_count == 0 && form->offset != 0))
	{
		purple_notify_error(gc, GGP_PUBDIR_SEARCH_TITLE,
			_("Error while searching for buddies"), NULL);
		ggp_pubdir_search_form_free(form);
		return;
	}
	
	if (records_count == 0)
	{
		purple_notify_info(gc, GGP_PUBDIR_SEARCH_TITLE,
			_("No matching users found"),
			_("There are no users matching your search criteria."));
		ggp_pubdir_search_form_free(form);
		return;
	}
	
	form->offset = next_offset;
	
	results = purple_notify_searchresults_new();
	
	purple_notify_searchresults_column_add(results,
		purple_notify_searchresults_column_new(_("GG Number")));
	purple_notify_searchresults_column_add(results,
		purple_notify_searchresults_column_new(_("Name")));
	purple_notify_searchresults_column_add(results,
		purple_notify_searchresults_column_new(_("City")));
	purple_notify_searchresults_column_add(results,
		purple_notify_searchresults_column_new(_("Gender")));
	purple_notify_searchresults_column_add(results,
		purple_notify_searchresults_column_new(_("Age")));
	
	for (i = 0; i < records_count; i++)
	{
		GList *row = NULL;
		const ggp_pubdir_record *record = &records[i];
		gchar *gender = NULL, *age = NULL;

		if (record->gender == GGP_PUBDIR_GENDER_MALE)
			gender = g_strdup("male");
		else if (record->gender == GGP_PUBDIR_GENDER_FEMALE)
			gender = g_strdup("female");
		
		if (record->age)
			age = g_strdup_printf("%d", record->age);
		
		row = g_list_append(row, g_strdup(ggp_uin_to_str(record->uin)));
		row = g_list_append(row, g_strdup(record->label));
		row = g_list_append(row, g_strdup(record->city));
		row = g_list_append(row, gender);
		row = g_list_append(row, age);
		purple_notify_searchresults_row_add(results, row);
	}
	
	purple_notify_searchresults_button_add(results,
		PURPLE_NOTIFY_BUTTON_ADD, ggp_pubdir_search_results_add);
	purple_notify_searchresults_button_add(results,
		PURPLE_NOTIFY_BUTTON_IM, ggp_pubdir_search_results_im);
	purple_notify_searchresults_button_add(results,
		PURPLE_NOTIFY_BUTTON_INFO, ggp_pubdir_search_results_info);
	purple_notify_searchresults_button_add_labeled(results, _("New search"),
		ggp_pubdir_search_results_new);
	if (next_offset != 0)
		purple_notify_searchresults_button_add(results,
			PURPLE_NOTIFY_BUTTON_CONTINUE,
			ggp_pubdir_search_results_next);
	
	if (!form->display_handle)
		form->display_handle = purple_notify_searchresults(gc,
			GGP_PUBDIR_SEARCH_TITLE, _("Search results"), NULL,
			results, ggp_pubdir_search_results_close, form);
	else
		purple_notify_searchresults_new_rows(gc, results,
			form->display_handle);
	g_assert(form->display_handle);
}

static void ggp_pubdir_search_results_close(gpointer _form)
{
	ggp_pubdir_search_form *form = _form;
	ggp_pubdir_search_form_free(form);
}

static void ggp_pubdir_search_results_next(PurpleConnection *gc, GList *row,
	gpointer _form)
{
	ggp_pubdir_search_form *form = _form;
	ggp_pubdir_search_execute(gc, form, ggp_pubdir_search_results_display,
		form);
}

static void ggp_pubdir_search_results_add(PurpleConnection *gc, GList *row,
	gpointer _form)
{
	purple_blist_request_add_buddy(purple_connection_get_account(gc),
		g_list_nth_data(row, 0), NULL, g_list_nth_data(row, 1));
}

static void ggp_pubdir_search_results_im(PurpleConnection *gc, GList *row,
	gpointer _form)
{
	purple_conversation_present(PURPLE_CONVERSATION(purple_im_conversation_new(
		purple_connection_get_account(gc), g_list_nth_data(row, 0))));
}

static void ggp_pubdir_search_results_info(PurpleConnection *gc, GList *row,
	gpointer _form)
{
	ggp_pubdir_get_info_protocol(gc, g_list_nth_data(row, 0));
}

static void ggp_pubdir_search_results_new(PurpleConnection *gc, GList *row,
	gpointer _form)
{
	ggp_pubdir_search_form *form = _form;
	ggp_pubdir_search(gc, form);
}

/*******************************************************************************
 * Own profile.
 ******************************************************************************/

void ggp_pubdir_set_info(PurpleConnection *gc)
{
	ggp_pubdir_get_info(gc, ggp_str_to_uin(purple_account_get_username(
		purple_connection_get_account(gc))),
		ggp_pubdir_set_info_dialog, NULL);
}

static void ggp_pubdir_set_info_dialog(PurpleConnection *gc, int records_count,
	const ggp_pubdir_record *records, int next_offset, void *user_data)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	int default_gender, i;
	const ggp_pubdir_record *record;
	
	purple_debug_info("gg", "ggp_pubdir_set_info_dialog (record: %d)\n",
		records_count);
	
	record = (records_count == 1 ? &records[0] : NULL);
	
	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);
	
	field = purple_request_field_string_new("first_name", _("First name"),
		record ? record->first_name : NULL, FALSE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("last_name", _("Last name"),
		record ? record->last_name : NULL, FALSE);
	purple_request_field_group_add_field(group, field);
	
	default_gender = -1;
	if (record && record->gender == GGP_PUBDIR_GENDER_MALE)
		default_gender = 0;
	else if (record && record->gender == GGP_PUBDIR_GENDER_FEMALE)
		default_gender = 1;
	
	field = purple_request_field_choice_new("gender", _("Gender"),
		default_gender);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_choice_add(field, _("Male"));
	purple_request_field_choice_add(field, _("Female"));
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("birth_date", _("Birth Day"),
		(record && record->birth) ?
		ggp_date_strftime("%Y-%m-%d", record->birth) : NULL, FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("city", _("City"),
		record ? record->city : NULL, FALSE);
	purple_request_field_group_add_field(group, field);
	
	field = purple_request_field_choice_new("province", _("Voivodeship"), 0);
	purple_request_field_group_add_field(group, field);
	purple_request_field_choice_add(field, _("Not specified"));
	for (i = 0; i < ggp_pubdir_provinces_count; i++)
	{
		purple_request_field_choice_add(field, ggp_pubdir_provinces[i]);
		if (record && i + 1 == record->province)
		{
			purple_request_field_choice_set_value(field, i + 1);
			purple_request_field_choice_set_default_value(field,
				i + 1); // TODO: libpurple bug?
		}
	}
	
	purple_request_fields(gc, _("Set User Info"), _("Set User Info"),
		NULL, fields,
		_("OK"), G_CALLBACK(ggp_pubdir_set_info_request),
		_("Cancel"), NULL,
		purple_connection_get_account(gc), NULL, NULL, gc);
	
}

static void ggp_pubdir_set_info_request(PurpleConnection *gc,
	PurpleRequestFields *fields)
{
	gchar *url;
	uin_t uin = ggp_str_to_uin(purple_account_get_username(
		purple_connection_get_account(gc)));
	ggp_pubdir_record *record = g_new0(ggp_pubdir_record, 1);
	gchar *birth_s;
	
	purple_debug_info("gg", "ggp_pubdir_set_info_request\n");

	record->uin = uin;
	record->first_name = g_strdup(purple_request_fields_get_string(fields,
		"first_name"));
	record->last_name = g_strdup(purple_request_fields_get_string(fields,
		"last_name"));
	if (purple_request_fields_get_choice(fields, "gender") == 0)
		record->gender = GGP_PUBDIR_GENDER_MALE;
	else
		record->gender = GGP_PUBDIR_GENDER_FEMALE;
	record->city = g_strdup(purple_request_fields_get_string(fields,
		"city"));
	record->province = purple_request_fields_get_choice(fields, "province");
	
	birth_s = g_strdup_printf("%sT10:00:00+00:00",
		purple_request_fields_get_string(fields, "birth_date"));
	record->birth = ggp_date_from_iso8601(birth_s);
	g_free(birth_s);
	purple_debug_info("gg", "ggp_pubdir_set_info_request: birth [%lu][%s]\n",
		record->birth, purple_request_fields_get_string(
		fields, "birth_date"));

	url = g_strdup_printf("http://api.gadu-gadu.pl/users/%u.xml", uin);
	ggp_oauth_request(gc, ggp_pubdir_set_info_got_token, record,
		"PUT", url);
	g_free(url);
}

static void ggp_pubdir_set_info_got_token(PurpleConnection *gc,
	const gchar *token, gpointer _record)
{
	PurpleHttpRequest *req;
	ggp_pubdir_record *record = _record;
	gchar *request_data;
	gchar *name, *surname, *city;
	uin_t uin = record->uin;

	if (!token || !PURPLE_CONNECTION_IS_VALID(gc))
	{
		// TODO: notify about failure
		ggp_pubdir_record_free(record, 1);
		return;
	}
	
	name = g_uri_escape_string(record->first_name, NULL, FALSE);
	surname = g_uri_escape_string(record->last_name, NULL, FALSE);
	city = g_uri_escape_string(record->city, NULL, FALSE);
	
	request_data = g_strdup_printf(
		"name=%s&"
		"surname=%s&"
		"birth=%sT10:00:00%%2B00:00&"
		"birth_priv=2&"
		"gender=%d&"
		"gender_priv=2&"
		"city=%s&"
		"province=%d",
		name, surname,
		ggp_date_strftime("%Y-%m-%d", record->birth),
		record->gender,
		city,
		record->province);

	if (purple_debug_is_verbose() && purple_debug_is_unsafe()) {
		purple_debug_misc("gg", "ggp_pubdir_set_info_got_token: "
			"query [%s]\n", request_data);
	}

	req = purple_http_request_new(NULL);
	purple_http_request_set_method(req, "PUT");
	purple_http_request_set_url_printf(req,
		"http://api.gadu-gadu.pl/users/%u.xml", uin);
	purple_http_request_header_set(req, "Authorization", token);
	purple_http_request_header_set(req, "Content-Type",
		"application/x-www-form-urlencoded");
	purple_http_request_set_contents(req, request_data, -1);
	purple_http_request(gc, req, ggp_pubdir_set_info_got_response, NULL);
	purple_http_request_unref(req);

	g_free(request_data);
	ggp_pubdir_record_free(record, 1);
}

static void ggp_pubdir_set_info_got_response(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer user_data)
{
	if (!purple_http_response_is_successful(response)) {
		purple_debug_error("gg", "ggp_pubdir_set_info_got_response: "
			"failed\n");
		return;
	}

	purple_debug_info("gg", "ggp_pubdir_set_info_got_response: [%s]\n",
		purple_http_response_get_data(response, NULL));
	/* <result><status>0</status></result> */

	/* TODO: notify about failure */
}
