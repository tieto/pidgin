#include "pubdir-prpl.h"

#include <debug.h>
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
static void ggp_pubdir_got_data(PurpleUtilFetchUrlData *url_data,
	gpointer user_data, const gchar *url_text, gsize len,
	const gchar *error_message);

static void ggp_pubdir_get_info_prpl_got(PurpleConnection *gc,
	int records_count, const ggp_pubdir_record *records, int next_offset,
	void *_uin);

static void ggp_pubdir_request_buddy_alias_got(PurpleConnection *gc,
	int records_count, const ggp_pubdir_record *records, int next_offset,
	void *_uin);

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

/******************************************************************************/

void ggp_pubdir_record_free(ggp_pubdir_record *records, int count)
{
	int i;
	for (i = 0; i < count; i++)
	{
		g_free(records[i].label);
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
	gchar *http_request;
	ggp_pubdir_request *request = _request;
	gchar *url;

	if (!token || !PURPLE_CONNECTION_IS_VALID(gc))
	{
		request->cb(gc, -1, NULL, 0, request->user_data);
		ggp_pubdir_request_free(request);
		return;
	}

	url = g_strdup_printf("http://api.gadu-gadu.pl/users/%u",
		request->params.user_info.uin);
	http_request = g_strdup_printf(
		"GET /users/%u HTTP/1.1\r\n"
		"Host: api.gadu-gadu.pl\r\n"
		"%s\r\n"
		"\r\n",
		request->params.user_info.uin,
		token);
	
	purple_util_fetch_url_request(purple_connection_get_account(gc), url,
		FALSE, NULL, TRUE, http_request, FALSE, -1,
		ggp_pubdir_got_data, request);
	
	g_free(url);
	g_free(http_request);
}

static void ggp_pubdir_got_data(PurpleUtilFetchUrlData *url_data,
	gpointer _request, const gchar *url_text, gsize len,
	const gchar *error_message)
{
	ggp_pubdir_request *request = _request;
	PurpleConnection *gc = request->gc;
	gboolean succ = TRUE;
	xmlnode *xml;
	unsigned int status, next_offset;
	int record_count, i;
	ggp_pubdir_record *records;

	//TODO: verbose
	//purple_debug_misc("gg", "ggp_pubdir_got_data: [%s]\n", url_text);

	xml = xmlnode_from_str(url_text, -1);
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
		gchar *label = NULL, *nick = NULL, *name = NULL,
			*surname = NULL, *city = NULL, *birth_s = NULL;
		unsigned int gender = 0;
		GTimeVal birth_g;
		g_assert(i <= record_count);
		
		record->uin = ggp_str_to_uin(xmlnode_get_attrib(xml, "uin"));
		if (record->uin == 0)
			ggp_xml_get_uint(xml, "uin", &record->uin);
		if (record->uin == 0)
			purple_debug_error("gg", "ggp_pubdir_got_data:"
				" invalid uin\n");
		
		ggp_xml_get_string(xml, "label", &label);
		ggp_xml_get_string(xml, "nick", &nick);
		ggp_xml_get_string(xml, "name", &name);
		ggp_xml_get_string(xml, "surname", &surname);
		ggp_xml_get_string(xml, "city", &city);
		ggp_xml_get_string(xml, "birth", &birth_s);
		ggp_xml_get_uint(xml, "gender", &gender);
		ggp_xml_get_uint(xml, "age", &record->age);
		
		if (label)
			record->label = g_strdup(label);
		else if (nick)
			record->label = g_strdup(nick);
		else if (name && surname)
			record->label = g_strdup_printf("%s %s", name, surname);
		else if (name)
			record->label = g_strdup(name);
		else if (surname)
			record->label = g_strdup(surname);
		if (record->label)
			g_strstrip(record->label);
		
		if (g_strcmp0(record->label, ggp_uin_to_str(record->uin)) == 0 ||
			g_strcmp0(record->label, "") == 0)
		{
			g_free(record->label);
			record->label = NULL;
		}
		
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
		
		if (birth_s && g_time_val_from_iso8601(birth_s, &birth_g))
			record->birth = birth_g.tv_sec;
		//TODO: calculate age from birth
		
		//TODO: verbose
		//purple_debug_misc("gg", "ggp_pubdir_got_data: [%d][%s][%s][%d][%d][%lu]\n",
		//	record->uin, record->label, record->city, record->gender, record->age, record->birth);
		
		g_free(label);
		g_free(nick);
		g_free(name);
		g_free(surname);
		g_free(city);
		
		xml = xmlnode_get_next_twin(xml);
	}
	
	request->cb(gc, record_count, records, next_offset, request->user_data);
	
	ggp_pubdir_request_free(request);
	ggp_pubdir_record_free(records, record_count);
}

void ggp_pubdir_get_info_prpl(PurpleConnection *gc, const char *name)
{
	uin_t uin = ggp_str_to_uin(name);

	purple_debug_info("gg", "ggp_pubdir_get_info_prpl: %u\n", uin);

	ggp_pubdir_get_info(gc, uin, ggp_pubdir_get_info_prpl_got, (void*)uin);
}

static void ggp_pubdir_get_info_prpl_got(PurpleConnection *gc,
	int records_count, const ggp_pubdir_record *records, int next_offset,
	void *_uin)
{
	uin_t uin = (uin_t)_uin;
	PurpleNotifyUserInfo *info = purple_notify_user_info_new();
	const ggp_pubdir_record *record = &records[0];
	PurpleBuddy *buddy;
	
	if (records_count < 1)
	{
		purple_debug_error("gg", "ggp_pubdir_get_info_prpl_got: "
			"couldn't get info for %u\n", uin);
		purple_notify_user_info_add_pair_plaintext(info, NULL,
			_("Cannot get user information"));
		purple_notify_userinfo(gc, ggp_uin_to_str(uin), info,
			NULL, NULL);
		purple_notify_user_info_destroy(info);
		return;
	}
	
	purple_debug_info("gg", "ggp_pubdir_get_info_prpl_got: %u\n", uin);
	g_assert(uin == record->uin);
	g_assert(records_count == 1);
	
	buddy = purple_find_buddy(purple_connection_get_account(gc),
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
	
	if (record->label)
		purple_notify_user_info_add_pair_plaintext(info, _("Name"),
			record->label);
	if (record->gender != GGP_PUBDIR_GENDER_UNSPECIFIED)
		purple_notify_user_info_add_pair_plaintext(info, _("Gender"),
			record->gender == GGP_PUBDIR_GENDER_FEMALE ?
			_("Female") : _("Male"));
	if (record->city)
		purple_notify_user_info_add_pair_plaintext(info, _("City"),
			record->city);
	if (record->birth)
	{
		GDate date;
		gchar buff[15];
		g_date_set_time(&date, record->birth);
		g_date_strftime(buff, sizeof(buff), "%Y-%m-%d", &date);
		purple_notify_user_info_add_pair_plaintext(info, _("Birthday"),
			buff);
	}
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

	ggp_pubdir_get_info(gc, uin, ggp_pubdir_request_buddy_alias_got, (void*)uin);
}

static void ggp_pubdir_request_buddy_alias_got(PurpleConnection *gc,
	int records_count, const ggp_pubdir_record *records, int next_offset,
	void *_uin)
{
	uin_t uin = (uin_t)_uin;
	const gchar *alias;
	
	if (records_count < 0)
	{
		purple_debug_error("gg", "ggp_pubdir_request_buddy_alias_got: "
			"couldn't get info for %u\n", uin);
		return;
	}
	g_assert(uin == records[0].uin);
	
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
	gchar *http_request;
	ggp_pubdir_request *request = _request;
	gchar *url;
	gchar *query;

	if (!token || !PURPLE_CONNECTION_IS_VALID(gc))
	{
		request->cb(gc, -1, NULL, 0, request->user_data);
		ggp_pubdir_request_free(request);
		return;
	}

	purple_debug_misc("gg", "ggp_pubdir_search_got_token\n");

	query = ggp_pubdir_search_make_query(request->params.search_form);
	url = g_strdup_printf("http://api.gadu-gadu.pl%s", query);
	http_request = g_strdup_printf(
		"GET %s HTTP/1.1\r\n"
		"Host: api.gadu-gadu.pl\r\n"
		"%s\r\n"
		"\r\n",
		query, token);
	g_free(query);
	
	purple_util_fetch_url_request(purple_connection_get_account(gc), url,
		FALSE, NULL, TRUE, http_request, FALSE, -1,
		ggp_pubdir_got_data, request);
	
	g_free(url);
	g_free(http_request);
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
	purple_conversation_present(purple_conversation_new(PURPLE_CONV_TYPE_IM,
		purple_connection_get_account(gc), g_list_nth_data(row, 0)));
}

static void ggp_pubdir_search_results_info(PurpleConnection *gc, GList *row,
	gpointer _form)
{
	ggp_pubdir_get_info_prpl(gc, g_list_nth_data(row, 0));
}

static void ggp_pubdir_search_results_new(PurpleConnection *gc, GList *row,
	gpointer _form)
{
	ggp_pubdir_search_form *form = _form;
	ggp_pubdir_search(gc, form);
}
