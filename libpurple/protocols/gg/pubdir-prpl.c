#include "pubdir-prpl.h"

#include <debug.h>

#include "oauth/oauth-purple.h"
#include "xml.h"
#include "utils.h"
#include "status.h"

typedef struct
{
	PurpleConnection *gc;
	ggp_pubdir_request_cb cb;
	void *user_data;
	union
	{
		struct
		{
			uin_t uin;
		} user_info;
	} params;
} ggp_pubdir_request;

void ggp_pubdir_request_free(ggp_pubdir_request *request);
void ggp_pubdir_record_free(ggp_pubdir_record *records, int count);

static void ggp_pubdir_get_info_got_token(PurpleConnection *gc,
	const gchar *token, gpointer _request);
static void ggp_pubdir_get_info_got_data(PurpleUtilFetchUrlData *url_data,
	gpointer user_data, const gchar *url_text, gsize len,
	const gchar *error_message);

static void ggp_pubdir_get_info_prpl_got(PurpleConnection *gc,
	int records_count, const ggp_pubdir_record *records, void *_uin);

static void ggp_pubdir_request_buddy_alias_got(PurpleConnection *gc,
	int records_count, const ggp_pubdir_record *records, void *_uin);

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
	g_free(request);
}

void ggp_pubdir_get_info(PurpleConnection *gc, uin_t uin,
	ggp_pubdir_request_cb cb, void *user_data)
{
	ggp_pubdir_request *request = g_new0(ggp_pubdir_request, 1);
	gchar *url;

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
		request->cb(gc, -1, NULL, request->user_data);
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
		ggp_pubdir_get_info_got_data, request);
	
	g_free(url);
	g_free(http_request);
}

static void ggp_pubdir_get_info_got_data(PurpleUtilFetchUrlData *url_data,
	gpointer _request, const gchar *url_text, gsize len,
	const gchar *error_message)
{
	ggp_pubdir_request *request = _request;
	PurpleConnection *gc = request->gc;
	gboolean succ = TRUE;
	xmlnode *xml;
	unsigned int status, nextOffset;
	int record_count, i;
	ggp_pubdir_record *records;

	purple_debug_misc("gg", "ggp_pubdir_get_info_got_data: [%s]\n", url_text);

	xml = xmlnode_from_str(url_text, -1);
	if (xml == NULL)
	{
		purple_debug_error("gg", "ggp_pubdir_get_info_got_data: "
			"invalid xml\n");
		request->cb(gc, -1, NULL, request->user_data);
		ggp_pubdir_request_free(request);
		return;
	}
	
	succ &= ggp_xml_get_uint(xml, "status", &status);
	if (!ggp_xml_get_uint(xml, "nextOffset", &nextOffset))
		nextOffset = 0;
	xml = xmlnode_get_child(xml, "users");
	if (!succ || status != 0 || !xml)
	{
		purple_debug_error("gg", "ggp_pubdir_get_info_got_data: "
			"invalid reply\n");
		request->cb(gc, -1, NULL, request->user_data);
		ggp_pubdir_request_free(request);
		return;
	}
	
	record_count = ggp_xml_child_count(xml, "user");
	records = g_new0(ggp_pubdir_record, record_count);
	
	purple_debug_info("gg", "ggp_pubdir_get_info_got_data: got %d\n", record_count);
	
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
		
		if (g_strcmp0(record->label, ggp_uin_to_str(record->uin)) == 0)
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
		
		if (birth_s && g_time_val_from_iso8601(birth_s, &birth_g))
			record->birth = birth_g.tv_sec;
		//TODO: calculate age from birth
		
		purple_debug_info("gg", "ggp_pubdir_get_info_got_data: [%d][%s][%s][%d][%lu]\n",
			record->uin, record->label, record->city, record->gender, record->birth);
		
		g_free(label);
		g_free(nick);
		g_free(name);
		g_free(surname);
		g_free(city);
		
		xml = xmlnode_get_next_twin(xml);
	}
	
	request->cb(gc, record_count, records, request->user_data);
	
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
	int records_count, const ggp_pubdir_record *records, void *_uin)
{
	uin_t uin = (uin_t)_uin;
	PurpleNotifyUserInfo *info = purple_notify_user_info_new();
	const ggp_pubdir_record *record = &records[0];
	PurpleBuddy *buddy;
	
	if (records_count < 0)
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
	int records_count, const ggp_pubdir_record *records, void *_uin)
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
