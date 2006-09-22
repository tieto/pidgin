/**
 * The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
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
 */

#include "internal.h"
#include "debug.h"
#include "notify.h"
#include "request.h"

#include "utils.h"
#include "packet_parse.h"
#include "buddy_info.h"		
#include "char_conv.h"
#include "crypt.h"
#include "header_info.h"
#include "keep_alive.h"
#include "send_core.h"

#define QQ_PRIMARY_INFORMATION _("Primary Information")
#define QQ_ADDITIONAL_INFORMATION _("Additional Information")
#define QQ_INTRO _("Personal Introduction")
#define QQ_NUMBER _("QQ Number")
#define QQ_NICKNAME _("Nickname")
#define QQ_NAME _("Name")
#define QQ_AGE _("Age")
#define QQ_GENDER _("Gender")
#define QQ_COUNTRY _("Country/Region")
#define QQ_PROVINCE _("Province/State")
#define QQ_CITY _("City")
#define QQ_HOROSCOPE _("Horoscope Symbol")
#define QQ_OCCUPATION _("Occupation")
#define QQ_ZODIAC _("Zodiac Sign")
#define QQ_BLOOD _("Blood Type")
#define QQ_COLLEGE _("College")
#define QQ_EMAIL _("Email")
#define QQ_ADDRESS _("Address")
#define QQ_ZIPCODE _("Zipcode")
#define QQ_CELL _("Cellphone Number")
#define QQ_TELEPHONE _("Phone Number")
#define QQ_HOMEPAGE _("Homepage")

#define QQ_HOROSCOPE_SIZE 13
static const gchar *horoscope_names[] = {
	"-", N_("Aquarius"), N_("Pisces"), N_("Aries"), N_("Taurus"),
	N_("Gemini"), N_("Cancer"), N_("Leo"), N_("Virgo"), N_("Libra"),
	N_("Scorpio"), N_("Sagittarius"), N_("Capricorn")
};

#define QQ_ZODIAC_SIZE 13
static const gchar *zodiac_names[] = {
	"-", N_("Rat"), N_("Ox"), N_("Tiger"), N_("Rabbit"),
	N_("Dragon"), N_("Snake"), N_("Horse"), N_("Goat"), N_("Monkey"),
	N_("Rooster"), N_("Dog"), N_("Pig")
};

#define QQ_BLOOD_SIZE 6
static const gchar *blood_types[] = {
	"-", "A", "B", "O", "AB", N_("Other")
};

#define QQ_GENDER_SIZE 2
static const gchar *genders[] = {
	N_("Male"),
	N_("Female")
};

#define QQ_CONTACT_FIELDS                               37

/* There is no user id stored in the reply packet for information query
 * we have to manually store the query, so that we know the query source */
typedef struct _qq_info_query {
        guint32 uid;
        gboolean show_window;
        gboolean modify_info;
} qq_info_query;

/* We get an info packet on ourselves before we modify our information.
 * Even though not all of the information is modifiable, it still
 * all needs to be there when we send out the modify info packet */
typedef struct _modify_info_data {
        GaimConnection *gc;
	contact_info *info;
} modify_info_data;

/* return -1 as a sentinel */
static gint choice_index(const gchar *value, const gchar **choice, gint choice_size)
{
	gint len, i;

	len = strlen(value);
	if (len > 3 || len == 0) return -1;
	for (i = 0; i < len; i++) {
		if (!g_ascii_isdigit(value[i])) 
			return -1;
	}
	i = strtol(value, NULL, 10);
	if (i >= choice_size)
		return -1;

	return i;
}

/* return should be freed */
static gchar *field_value(const gchar *field, const gchar **choice, gint choice_size)
{
	gint index, len;

	len = strlen(field);
	if (len == 0) {
		return NULL;
	} else if (choice != NULL) {
		/* some choice fields are also customizable */
		index = choice_index(field, choice, choice_size);
		if (index == -1) {
			if (strcmp(field, "-") != 0) {
				return qq_to_utf8(field, QQ_CHARSET_DEFAULT);
			} else {
				return NULL;
			}
		/* else ASCIIized index */
		} else {
			if (strcmp(choice[index], "-") != 0)
				return g_strdup(choice[index]);
			else
				return NULL;
		}
	} else {
		if (strcmp(field, "-") != 0) {
			return qq_to_utf8(field, QQ_CHARSET_DEFAULT);
		} else {
			return NULL;
		}
	}
}

static void append_field_value(GString *info_text, const gchar *field, 
		const gchar *title, const gchar **choice, gint choice_size)
{
	gchar *value = field_value(field, choice, choice_size);
	
	if (value != NULL) {
		g_string_append_printf(info_text, "<br /><b>%s:</b> %s", title, value);
		g_free(value);
	}
}

static GString *info_to_str(const contact_info *info)
{
	GString *info_text, *extra_info;
	const gchar *intro;
	gint len;

	info_text = g_string_new("");
	g_string_append_printf(info_text, "<b>%s</b><br /><br />", QQ_PRIMARY_INFORMATION);
	g_string_append_printf(info_text, "<b>%s:</b> %s", QQ_NUMBER, info->uid);
	append_field_value(info_text, info->nick, QQ_NICKNAME, NULL, 0);
	append_field_value(info_text, info->name, QQ_NAME, NULL, 0);
	append_field_value(info_text, info->age, QQ_AGE, NULL, 0);
	append_field_value(info_text, info->gender, QQ_GENDER, genders, QQ_GENDER_SIZE);
	append_field_value(info_text, info->country, QQ_COUNTRY, NULL, 0);
	append_field_value(info_text, info->province, QQ_PROVINCE, NULL, 0);
	append_field_value(info_text, info->city, QQ_CITY, NULL, 0);

	extra_info = g_string_new("");
	g_string_append_printf(extra_info, "<br /><br /><b>%s</b><br />", QQ_ADDITIONAL_INFORMATION);
	len = extra_info->len;
	append_field_value(extra_info, info->horoscope, QQ_HOROSCOPE, horoscope_names, QQ_HOROSCOPE_SIZE);
	append_field_value(extra_info, info->occupation, QQ_OCCUPATION, NULL, 0);
	append_field_value(extra_info, info->zodiac, QQ_ZODIAC, zodiac_names, QQ_ZODIAC_SIZE);
	append_field_value(extra_info, info->blood, QQ_BLOOD, blood_types, QQ_BLOOD_SIZE);
	append_field_value(extra_info, info->college, QQ_COLLEGE, NULL, 0);
	append_field_value(extra_info, info->email, QQ_EMAIL, NULL, 0);
	append_field_value(extra_info, info->address, QQ_ADDRESS, NULL, 0);
	append_field_value(extra_info, info->zipcode, QQ_ZIPCODE, NULL, 0);
	append_field_value(extra_info, info->hp_num, QQ_CELL, NULL, 0);
	append_field_value(extra_info, info->tel, QQ_TELEPHONE, NULL, 0);
	append_field_value(extra_info, info->homepage, QQ_HOMEPAGE, NULL, 0);
	if (len != extra_info->len)
		g_string_append(info_text, extra_info->str);
	g_string_free(extra_info, TRUE);

	intro = field_value(info->intro, NULL, 0);
	if (intro) {
		g_string_append_printf(info_text, "<br /><br /><b>%s</b><br /><br />", QQ_INTRO);
		g_string_append(info_text, intro);
	}

	/* for debugging */
	/*
	g_string_append_printf(info_text, "<br /><br /><b>%s</b><br />", "Miscellaneous");
	append_field_value(info_text, info->pager_sn, "pager_sn", NULL, 0);
	append_field_value(info_text, info->pager_num, "pager_num", NULL, 0);
	append_field_value(info_text, info->pager_sp, "pager_sp", NULL, 0);
	append_field_value(info_text, info->pager_base_num, "pager_base_num", NULL, 0);
	append_field_value(info_text, info->pager_type, "pager_type", NULL, 0);
	append_field_value(info_text, info->auth_type, "auth_type", NULL, 0);
	append_field_value(info_text, info->unknown1, "unknown1", NULL, 0);
	append_field_value(info_text, info->unknown2, "unknown2", NULL, 0);
	append_field_value(info_text, info->face, "face", NULL, 0);
	append_field_value(info_text, info->hp_type, "hp_type", NULL, 0);
	append_field_value(info_text, info->unknown3, "unknown3", NULL, 0);
	append_field_value(info_text, info->unknown4, "unknown4", NULL, 0);
	append_field_value(info_text, info->unknown5, "unknown5", NULL, 0);
	append_field_value(info_text, info->is_open_hp, "is_open_hp", NULL, 0);
	append_field_value(info_text, info->is_open_contact, "is_open_contact", NULL, 0);
	append_field_value(info_text, info->qq_show, "qq_show", NULL, 0);
	append_field_value(info_text, info->unknown6, "unknown6", NULL, 0);
	*/

	return info_text;
}

/* send a packet to get detailed information of uid */
void qq_send_packet_get_info(GaimConnection *gc, guint32 uid, gboolean show_window)
{
	qq_data *qd;
	gchar uid_str[11];
	qq_info_query *query;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL && uid != 0);

	qd = (qq_data *) gc->proto_data;
	g_snprintf(uid_str, sizeof(uid_str), "%d", uid);
	qq_send_cmd(gc, QQ_CMD_GET_USER_INFO, TRUE, 0, TRUE, (guint8 *) uid_str, strlen(uid_str));

	query = g_new0(qq_info_query, 1);
	query->uid = uid;
	query->show_window = show_window;
	query->modify_info = FALSE;
	qd->info_query = g_list_append(qd->info_query, query);
}

/* set up the fields requesting personal information and send a get_info packet
 * for myself */
void qq_prepare_modify_info(GaimConnection *gc)
{
	qq_data *qd;
	GList *ql;
	qq_info_query *query;

	qd = (qq_data *) gc->proto_data;
	qq_send_packet_get_info(gc, qd->uid, FALSE);
	/* traverse backwards so we get the most recent info_query */
	for (ql = g_list_last(qd->info_query); ql != NULL; ql = g_list_previous(ql)) {
		query = ql->data;
		if (query->uid == qd->uid) 
			query->modify_info = TRUE;
	}
}

/* send packet to modify personal information */
static void qq_send_packet_modify_info(GaimConnection *gc, gchar **segments)
{
	gint i;
	guint8 *raw_data, *cursor, bar;

	g_return_if_fail(gc != NULL && segments != NULL);

	bar = 0x1f;
	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 128);
	cursor = raw_data;

	create_packet_b(raw_data, &cursor, bar);

	/* important! skip the first uid entry */
	for (i = 1; i < QQ_CONTACT_FIELDS; i++) {
		create_packet_b(raw_data, &cursor, bar);
		create_packet_data(raw_data, &cursor, (guint8 *) segments[i], strlen(segments[i]));
	}
	create_packet_b(raw_data, &cursor, bar);

	qq_send_cmd(gc, QQ_CMD_UPDATE_INFO, TRUE, 0, TRUE, raw_data, cursor - raw_data);

}

static void modify_info_cancel_cb(modify_info_data *mid)
{
	qq_data *qd;

	qd = (qq_data *) mid->gc->proto_data;
	qd->modifying_info = FALSE;

	g_strfreev((gchar **) mid->info);
	g_free(mid);
}

static gchar *parse_field(GList **list, gboolean choice)
{
	gchar *value;
	GaimRequestField *field;

	field = (GaimRequestField *) (*list)->data;
	if (choice) {
		value = g_strdup_printf("%d", gaim_request_field_choice_get_value(field));
	} else {
		value = (gchar *) gaim_request_field_string_get_value(field);
		if (value == NULL) 
			value = g_strdup("-");
		else 
			value = utf8_to_qq(value, QQ_CHARSET_DEFAULT);
	}
	*list = g_list_remove_link(*list, *list);

	return value;
}

/* parse fields and send info packet */
static void modify_info_ok_cb(modify_info_data *mid, GaimRequestFields *fields)
{
	GaimConnection *gc;
	qq_data *qd;
	GList *list,  *groups;
	contact_info *info;

	gc = mid->gc;
	qd = (qq_data *) gc->proto_data;
	qd->modifying_info = FALSE;

	info = mid->info;

	groups = gaim_request_fields_get_groups(fields);
	list = gaim_request_field_group_get_fields(groups->data);
	info->uid = parse_field(&list, FALSE);
	info->nick = parse_field(&list, FALSE);
	info->name = parse_field(&list, FALSE);
	info->age = parse_field(&list, FALSE);
	info->gender = parse_field(&list, TRUE);
	info->country = parse_field(&list, FALSE);
	info->province = parse_field(&list, FALSE);
	info->city = parse_field(&list, FALSE);
	groups = g_list_remove_link(groups, groups);
	list = gaim_request_field_group_get_fields(groups->data);
	info->horoscope = parse_field(&list, TRUE);
	info->occupation = parse_field(&list, FALSE);
	info->zodiac = parse_field(&list, TRUE);
	info->blood = parse_field(&list, TRUE);
	info->college = parse_field(&list, FALSE);
	info->email = parse_field(&list, FALSE);
	info->address = parse_field(&list, FALSE);
	info->zipcode = parse_field(&list, FALSE);
	info->hp_num = parse_field(&list, FALSE);
	info->tel = parse_field(&list, FALSE);
	info->homepage = parse_field(&list, FALSE);
	groups = g_list_remove_link(groups, groups);
	list = gaim_request_field_group_get_fields(groups->data);
	info->intro = parse_field(&list, FALSE);
	groups = g_list_remove_link(groups, groups);

	qq_send_packet_modify_info(gc, (gchar **) info);

	g_strfreev((gchar **) mid->info);
	g_free(mid);
}

static GaimRequestFieldGroup *setup_field_group(GaimRequestFields *fields, const gchar *title)
{
	GaimRequestFieldGroup *group;

	group = gaim_request_field_group_new(title);
	gaim_request_fields_add_group(fields, group);

	return group;
}

static void add_string_field_to_group(GaimRequestFieldGroup *group, 
		const gchar *id, const gchar *title, const gchar *value)
{
	GaimRequestField *field;
	gchar *utf8_value;
       
	utf8_value = qq_to_utf8(value, QQ_CHARSET_DEFAULT);
	field = gaim_request_field_string_new(id, title, utf8_value, FALSE);
	gaim_request_field_group_add_field(group, field);
	g_free(utf8_value);
}

static void add_choice_field_to_group(GaimRequestFieldGroup *group, 
		const gchar *id, const gchar *title, const gchar *value, 
		const gchar **choice, gint choice_size)
{
	GaimRequestField *field;
	gint i, index;
	
	index = choice_index(value, choice, choice_size);
	field = gaim_request_field_choice_new(id, title, index);
	for (i = 0; i < choice_size; i++)
		gaim_request_field_choice_add(field, choice[i]);
	gaim_request_field_group_add_field(group, field);
}

/* take the info returned by a get_info packet for myself and set up a request form */
static void create_modify_info_dialogue(GaimConnection *gc, const contact_info *info)
{
	qq_data *qd;
	GaimRequestFieldGroup *group;
	GaimRequestFields *fields;
	GaimRequestField *field;
	modify_info_data *mid;

	/* so we only have one dialog open at a time */
	qd = (qq_data *) gc->proto_data;
	if (!qd->modifying_info) {
		qd->modifying_info = TRUE;

		fields = gaim_request_fields_new();

		group = setup_field_group(fields, QQ_PRIMARY_INFORMATION);
		field = gaim_request_field_string_new("uid", QQ_NUMBER, info->uid, FALSE);
		gaim_request_field_group_add_field(group, field);
		gaim_request_field_string_set_editable(field, FALSE);
		add_string_field_to_group(group, "nick", QQ_NICKNAME, info->nick);
		add_string_field_to_group(group, "name", QQ_NAME, info->name);
		add_string_field_to_group(group, "age", QQ_AGE, info->age);
		add_choice_field_to_group(group, "gender", QQ_GENDER, info->gender, genders, QQ_GENDER_SIZE);
		add_string_field_to_group(group, "country", QQ_COUNTRY, info->country);
		add_string_field_to_group(group, "province", QQ_PROVINCE, info->province);
		add_string_field_to_group(group, "city", QQ_CITY, info->city);
		group = setup_field_group(fields, QQ_ADDITIONAL_INFORMATION);
		add_choice_field_to_group(group, "horoscope", QQ_HOROSCOPE, info->horoscope, horoscope_names, QQ_HOROSCOPE_SIZE);
		add_string_field_to_group(group, "occupation", QQ_OCCUPATION, info->occupation);
		add_choice_field_to_group(group, "zodiac", QQ_ZODIAC, info->zodiac, zodiac_names, QQ_ZODIAC_SIZE);
		add_choice_field_to_group(group, "blood", QQ_BLOOD, info->blood, blood_types, QQ_BLOOD_SIZE);
		add_string_field_to_group(group, "college", QQ_COLLEGE, info->college);
		add_string_field_to_group(group, "email", QQ_EMAIL, info->email);
		add_string_field_to_group(group, "address", QQ_ADDRESS, info->address);
		add_string_field_to_group(group, "zipcode", QQ_ZIPCODE, info->zipcode);
		add_string_field_to_group(group, "hp_num", QQ_CELL, info->hp_num);
		add_string_field_to_group(group, "tel", QQ_TELEPHONE, info->tel);
		add_string_field_to_group(group, "homepage", QQ_HOMEPAGE, info->homepage);

		group = setup_field_group(fields, QQ_INTRO);
		field = gaim_request_field_string_new("intro", QQ_INTRO, info->intro, TRUE);
		gaim_request_field_group_add_field(group, field);

		/* prepare unmodifiable info */
		mid = g_new0(modify_info_data, 1);
		mid->gc = gc;
		/* QQ_CONTACT_FIELDS+1 so that the array is NULL-terminated and can be g_strfreev()'ed later */
		mid->info = (contact_info *) g_new0(gchar *, QQ_CONTACT_FIELDS+1);
		mid->info->pager_sn = g_strdup(info->pager_sn);
		mid->info->pager_num = g_strdup(info->pager_num);
		mid->info->pager_sp = g_strdup(info->pager_sp);
		mid->info->pager_base_num = g_strdup(info->pager_base_num);
		mid->info->pager_type = g_strdup(info->pager_type);
		mid->info->auth_type = g_strdup(info->auth_type);
		mid->info->unknown1 = g_strdup(info->unknown1);
		mid->info->unknown2 = g_strdup(info->unknown2);
		mid->info->face = g_strdup(info->face);
		mid->info->hp_type = g_strdup(info->hp_type);
		mid->info->unknown3 = g_strdup(info->unknown3);
		mid->info->unknown4 = g_strdup(info->unknown4);
		mid->info->unknown5 = g_strdup(info->unknown5);
		/* TODO stop hiding these 2 */
		mid->info->is_open_hp = g_strdup(info->is_open_hp);
		mid->info->is_open_contact = g_strdup(info->is_open_contact);
		mid->info->qq_show = g_strdup(info->qq_show);
		mid->info->unknown6 = g_strdup(info->unknown6);
	
		gaim_request_fields(gc, _("Modify my information"),
			_("Modify my information"), NULL, fields,
			_("Update my information"), G_CALLBACK(modify_info_ok_cb),
			_("Cancel"), G_CALLBACK(modify_info_cancel_cb),
			mid);
	}
}

/* process the reply of modify_info packet */
void qq_process_modify_info_reply(guint8 *buf, gint buf_len, GaimConnection *gc)
{
	qq_data *qd;
	gint len;
	guint8 *data;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		data[len] = '\0';
		if (qd->uid == atoi((gchar *) data)) {	/* return should be my uid */
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "Update info ACK OK\n");
			gaim_notify_info(gc, NULL, _("Your information has been updated"), NULL);
		}
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt modify info reply\n");
	}
}

/* after getting info or modify myself, refresh the buddy list accordingly */
void qq_refresh_buddy_and_myself(contact_info *info, GaimConnection *gc)
{
	GaimBuddy *b;
	qq_data *qd;
	qq_buddy *q_bud;
	gchar *alias_utf8;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	alias_utf8 = qq_to_utf8(info->nick, QQ_CHARSET_DEFAULT);
	if (qd->uid == strtol(info->uid, NULL, 10)) {	/* it is me */
		qd->my_icon = strtol(info->face, NULL, 10);
		if (alias_utf8 != NULL)
			gaim_account_set_alias(gc->account, alias_utf8);
	}
	/* update buddy list (including myself, if myself is the buddy) */
	b = gaim_find_buddy(gc->account, uid_to_gaim_name(strtol(info->uid, NULL, 10)));
	q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;
	if (q_bud != NULL) {	/* I have this buddy */
		q_bud->age = strtol(info->age, NULL, 10);
		q_bud->gender = strtol(info->gender, NULL, 10);
		q_bud->icon = strtol(info->face, NULL, 10);
		if (alias_utf8 != NULL)
			q_bud->nickname = g_strdup(alias_utf8);
		qq_update_buddy_contact(gc, q_bud);
	}
	g_free(alias_utf8);
}

/* process reply to get_info packet */
void qq_process_get_info_reply(guint8 *buf, gint buf_len, GaimConnection *gc)
{
	gint len;
	guint8 *data;
	gchar **segments;
	qq_info_query *query;
	qq_data *qd;
	contact_info *info;
	GList *list, *query_list;
	GString *info_text;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	list = query_list = NULL;
	len = buf_len;
	data = g_newa(guint8, len);
	info = NULL;

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		if (NULL == (segments = split_data(data, len, "\x1e", QQ_CONTACT_FIELDS)))
			return;

		info = (contact_info *) segments;
		if (qd->modifying_face && strtol(info->face, NULL, 10) != qd->my_icon) {
			gchar *icon = g_strdup_printf("%i", qd->my_icon);
			qd->modifying_face = FALSE;
			memcpy(info->face, icon, 2);
			qq_send_packet_modify_info(gc, segments);
			g_free(icon);
		}

		qq_refresh_buddy_and_myself(info, gc);

		query_list = qd->info_query;
		/* ensure we're processing the right query */
		while (query_list) {
			query = (qq_info_query *) query_list->data;
			if (query->uid == atoi(info->uid)) {
				if (query->show_window) {
					info_text = info_to_str(info);
					gaim_notify_userinfo(gc, info->uid, info_text->str, NULL, NULL);
					g_string_free(info_text, TRUE);
				} else if (query->modify_info) {
					create_modify_info_dialogue(gc, info);
				}
				qd->info_query = g_list_remove(qd->info_query, qd->info_query->data);
				g_free(query);
				break;
			}
			query_list = query_list->next;
		}

		g_strfreev(segments);
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt get info reply\n");
	}
}

void qq_info_query_free(qq_data *qd)
{
	gint i;
	qq_info_query *p;

	g_return_if_fail(qd != NULL);

	i = 0;
	while (qd->info_query != NULL) {
		p = (qq_info_query *) (qd->info_query->data);
		qd->info_query = g_list_remove(qd->info_query, p);
		g_free(p);
		i++;
	}
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "%d info queries are freed!\n", i);
}
