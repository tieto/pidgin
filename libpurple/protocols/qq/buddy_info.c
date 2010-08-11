/**
 * @file buddy_info.c
 *
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
 */

#include "internal.h"
#include "debug.h"
#include "notify.h"
#include "request.h"

#include "utils.h"
#include "packet_parse.h"
#include "buddy_list.h"
#include "buddy_info.h"
#include "char_conv.h"
#include "header_info.h"
#include "qq_base.h"
#include "qq_network.h"

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
#define QQ_FACES	    100

/* There is no user id stored in the reply packet for information query
 * we have to manually store the query, so that we know the query source */
typedef struct _qq_info_query {
	guint32 uid;
	gboolean show_window;
	gboolean modify_info;
} qq_info_query;

typedef struct _contact_info {
	gchar *uid;
	gchar *nick;
	gchar *country;
	gchar *province;
	gchar *zipcode;
	gchar *address;
	gchar *tel;
	gchar *age;
	gchar *gender;
	gchar *name;
	gchar *email;
	gchar *pager_sn;
	gchar *pager_num;
	gchar *pager_sp;
	gchar *pager_base_num;
	gchar *pager_type;
	gchar *occupation;
	gchar *homepage;
	gchar *auth_type;
	gchar *unknown1;
	gchar *unknown2;
	gchar *face;
	gchar *hp_num;
	gchar *hp_type;
	gchar *intro;
	gchar *city;
	gchar *unknown3;
	gchar *unknown4;
	gchar *unknown5;
	gchar *is_open_hp;
	gchar *is_open_contact;
	gchar *college;
	gchar *horoscope;
	gchar *zodiac;
	gchar *blood;
	gchar *qq_show;
	gchar *unknown6;        /* always 0x2D */
} contact_info;

/* We get an info packet on ourselves before we modify our information.
 * Even though not all of the information is modifiable, it still
 * all needs to be there when we send out the modify info packet */
typedef struct _modify_info_data {
	PurpleConnection *gc;
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

static gboolean append_field_value(PurpleNotifyUserInfo *user_info, const gchar *field,
		const gchar *title, const gchar **choice, gint choice_size)
{
	gchar *value = field_value(field, choice, choice_size);

	if (value != NULL) {
		purple_notify_user_info_add_pair(user_info, title, value);
		g_free(value);

		return TRUE;
	}

	return FALSE;
}

static PurpleNotifyUserInfo *
	info_to_notify_user_info(const contact_info *info)
{
	PurpleNotifyUserInfo *user_info = purple_notify_user_info_new();
	const gchar *intro;
	gboolean has_extra_info = FALSE;

	purple_notify_user_info_add_pair(user_info, QQ_NUMBER, info->uid);

	append_field_value(user_info, info->nick, QQ_NICKNAME, NULL, 0);
	append_field_value(user_info, info->name, QQ_NAME, NULL, 0);
	append_field_value(user_info, info->age, QQ_AGE, NULL, 0);
	append_field_value(user_info, info->gender, QQ_GENDER, genders, QQ_GENDER_SIZE);
	append_field_value(user_info, info->country, QQ_COUNTRY, NULL, 0);
	append_field_value(user_info, info->province, QQ_PROVINCE, NULL, 0);
	append_field_value(user_info, info->city, QQ_CITY, NULL, 0);

	purple_notify_user_info_add_section_header(user_info, QQ_ADDITIONAL_INFORMATION);

	has_extra_info |= append_field_value(user_info, info->horoscope, QQ_HOROSCOPE, horoscope_names, QQ_HOROSCOPE_SIZE);
	has_extra_info |= append_field_value(user_info, info->occupation, QQ_OCCUPATION, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->zodiac, QQ_ZODIAC, zodiac_names, QQ_ZODIAC_SIZE);
	has_extra_info |= append_field_value(user_info, info->blood, QQ_BLOOD, blood_types, QQ_BLOOD_SIZE);
	has_extra_info |= append_field_value(user_info, info->college, QQ_COLLEGE, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->email, QQ_EMAIL, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->address, QQ_ADDRESS, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->zipcode, QQ_ZIPCODE, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->hp_num, QQ_CELL, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->tel, QQ_TELEPHONE, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->homepage, QQ_HOMEPAGE, NULL, 0);

	if (!has_extra_info)
		purple_notify_user_info_remove_last_item(user_info);

	intro = field_value(info->intro, NULL, 0);
	if (intro) {
		purple_notify_user_info_add_pair(user_info, QQ_INTRO, intro);
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

	return user_info;
}

/* send a packet to get detailed information of uid */
void qq_send_packet_get_info(PurpleConnection *gc, guint32 uid, gboolean show_window)
{
	qq_data *qd;
	gchar uid_str[11];
	qq_info_query *query;

	g_return_if_fail(uid != 0);

	qd = (qq_data *) gc->proto_data;
	g_snprintf(uid_str, sizeof(uid_str), "%d", uid);
	qq_send_cmd(qd, QQ_CMD_GET_USER_INFO, (guint8 *) uid_str, strlen(uid_str));

	query = g_new0(qq_info_query, 1);
	query->uid = uid;
	query->show_window = show_window;
	query->modify_info = FALSE;
	qd->info_query = g_list_append(qd->info_query, query);
}

/* set up the fields requesting personal information and send a get_info packet
 * for myself */
void qq_prepare_modify_info(PurpleConnection *gc)
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
static void qq_send_packet_modify_info(PurpleConnection *gc, contact_info *info)
{
	qq_data *qd = (qq_data *) gc->proto_data;
	gint bytes = 0;
	guint8 raw_data[MAX_PACKET_SIZE - 128] = {0};
	guint8 bar;

	g_return_if_fail(info != NULL);

	bar = 0x1f;

	bytes += qq_put8(raw_data + bytes, bar);

	/* important! skip the first uid entry */
	/*
	   for (i = 1; i < QQ_CONTACT_FIELDS; i++) {
	   create_packet_b(raw_data, &cursor, bar);
	   create_packet_data(raw_data, &cursor, (guint8 *) segments[i], strlen(segments[i]));
	   }
	   */
	/* uid */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->uid, strlen(info->uid));
	/* nick */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->nick, strlen(info->nick));
	/* country */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->country, strlen(info->country));
	/* province */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->province, strlen(info->province));
	/* zipcode */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->zipcode, strlen(info->zipcode));
	/* address */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->address, strlen(info->address));
	/* tel */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->tel, strlen(info->tel));
	/* age */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->age, strlen(info->age));
	/* gender */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->gender, strlen(info->gender));
	/* name */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->name, strlen(info->name));
	/* email */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->email, strlen(info->email));
	/* pager_sn */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->pager_sn, strlen(info->pager_sn));
	/* pager_num */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->pager_num, strlen(info->pager_num));
	/* pager_sp */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->pager_sp, strlen(info->pager_sp));
	/* pager_base_num */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->pager_base_num, strlen(info->pager_base_num));
	/* pager_type */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->pager_type, strlen(info->pager_type));
	/* occupation */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->occupation, strlen(info->occupation));
	/* homepage */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->homepage, strlen(info->homepage));
	/* auth_type */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->auth_type, strlen(info->auth_type));
	/* unknown1 */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->unknown1, strlen(info->unknown1));
	/* unknown2 */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->unknown2, strlen(info->unknown2));
	/* face */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->face, strlen(info->face));
	/* hp_num */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->hp_num, strlen(info->hp_num));
	/* hp_type */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->hp_type, strlen(info->hp_type));
	/* intro */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->intro, strlen(info->intro));
	/* city */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->city, strlen(info->city));
	/* unknown3 */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->unknown3, strlen(info->unknown3));
	/* unknown4 */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->unknown4, strlen(info->unknown4));
	/* unknown5 */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->unknown5, strlen(info->unknown5));
	/* is_open_hp */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->is_open_hp, strlen(info->is_open_hp));
	/* is_open_contact */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->is_open_contact, strlen(info->is_open_contact));
	/* college */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->college, strlen(info->college));
	/* horoscope */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->horoscope, strlen(info->horoscope));
	/* zodiac */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->zodiac, strlen(info->zodiac));
	/* blood */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->blood, strlen(info->blood));
	/* qq_show */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->qq_show, strlen(info->qq_show));
	/* unknown6 */
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)info->unknown6, strlen(info->unknown6));

	bytes += qq_put8(raw_data + bytes, bar);

	qq_send_cmd(qd, QQ_CMD_UPDATE_INFO, raw_data, bytes);

}

static void modify_info_cancel_cb(modify_info_data *mid)
{
	qq_data *qd;

	qd = (qq_data *) mid->gc->proto_data;
	qd->modifying_info = FALSE;

	g_strfreev((gchar **) mid->info);
	g_free(mid);
}

static gchar *parse_field(PurpleRequestField *field, gboolean choice)
{
	gchar *value;

	if (choice) {
		value = g_strdup_printf("%d", purple_request_field_choice_get_value(field));
	} else {
		value = (gchar *) purple_request_field_string_get_value(field);
		if (value == NULL)
			value = g_strdup("-");
		else
			value = utf8_to_qq(value, QQ_CHARSET_DEFAULT);
	}

	return value;
}

/* parse fields and send info packet */
static void modify_info_ok_cb(modify_info_data *mid, PurpleRequestFields *fields)
{
	PurpleConnection *gc;
	qq_data *qd;
	GList *groups;
	contact_info *info;

	gc = mid->gc;
	qd = (qq_data *) gc->proto_data;
	qd->modifying_info = FALSE;

	info = mid->info;

	groups = purple_request_fields_get_groups(fields);
	while (groups != NULL) {
		PurpleRequestFieldGroup *group = groups->data;
		const char *g_name = purple_request_field_group_get_title(group);
		GList *fields = purple_request_field_group_get_fields(group);

		if (g_name == NULL)
			continue;

		while (fields != NULL) {
			PurpleRequestField *field = fields->data;
			const char *f_id = purple_request_field_get_id(field);

			if (!strcmp(QQ_PRIMARY_INFORMATION, g_name)) {

				if (!strcmp(f_id, "uid"))
					info->uid = parse_field(field, FALSE);
				else if (!strcmp(f_id, "nick"))
					info->nick = parse_field(field, FALSE);
				else if (!strcmp(f_id, "name"))
					info->name = parse_field(field, FALSE);
				else if (!strcmp(f_id, "age"))
					info->age = parse_field(field, FALSE);
				else if (!strcmp(f_id, "gender"))
					info->gender = parse_field(field, TRUE);
				else if (!strcmp(f_id, "country"))
					info->country = parse_field(field, FALSE);
				else if (!strcmp(f_id, "province"))
					info->province = parse_field(field, FALSE);
				else if (!strcmp(f_id, "city"))
					info->city = parse_field(field, FALSE);

			} else if (!strcmp(QQ_ADDITIONAL_INFORMATION, g_name)) {

				if (!strcmp(f_id, "horoscope"))
					info->horoscope = parse_field(field, TRUE);
				else if (!strcmp(f_id, "occupation"))
					info->occupation = parse_field(field, FALSE);
				else if (!strcmp(f_id, "zodiac"))
					info->zodiac = parse_field(field, TRUE);
				else if (!strcmp(f_id, "blood"))
					info->blood = parse_field(field, TRUE);
				else if (!strcmp(f_id, "college"))
					info->college = parse_field(field, FALSE);
				else if (!strcmp(f_id, "email"))
					info->email = parse_field(field, FALSE);
				else if (!strcmp(f_id, "address"))
					info->address = parse_field(field, FALSE);
				else if (!strcmp(f_id, "zipcode"))
					info->zipcode = parse_field(field, FALSE);
				else if (!strcmp(f_id, "hp_num"))
					info->hp_num = parse_field(field, FALSE);
				else if (!strcmp(f_id, "tel"))
					info->tel = parse_field(field, FALSE);
				else if (!strcmp(f_id, "homepage"))
					info->homepage = parse_field(field, FALSE);

			} else if (!strcmp(QQ_INTRO, g_name)) {

				if (!strcmp(f_id, "intro"))
					info->intro = parse_field(field, FALSE);

			}

			fields = fields->next;
		}

		groups = groups->next;
	}

	/* This casting looks like a horrible idea to me -DAA
	 * yes, rewritten -s3e
	 * qq_send_packet_modify_info(gc, (gchar **) info);
	 */
	qq_send_packet_modify_info(gc, info);

	g_strfreev((gchar **) mid->info);
	g_free(mid);
}

static PurpleRequestFieldGroup *setup_field_group(PurpleRequestFields *fields, const gchar *title)
{
	PurpleRequestFieldGroup *group;

	group = purple_request_field_group_new(title);
	purple_request_fields_add_group(fields, group);

	return group;
}

static void add_string_field_to_group(PurpleRequestFieldGroup *group,
		const gchar *id, const gchar *title, const gchar *value)
{
	PurpleRequestField *field;
	gchar *utf8_value;

	utf8_value = qq_to_utf8(value, QQ_CHARSET_DEFAULT);
	field = purple_request_field_string_new(id, title, utf8_value, FALSE);
	purple_request_field_group_add_field(group, field);
	g_free(utf8_value);
}

static void add_choice_field_to_group(PurpleRequestFieldGroup *group,
		const gchar *id, const gchar *title, const gchar *value,
		const gchar **choice, gint choice_size)
{
	PurpleRequestField *field;
	gint i, index;

	index = choice_index(value, choice, choice_size);
	field = purple_request_field_choice_new(id, title, index);
	for (i = 0; i < choice_size; i++)
		purple_request_field_choice_add(field, choice[i]);
	purple_request_field_group_add_field(group, field);
}

/* take the info returned by a get_info packet for myself and set up a request form */
static void create_modify_info_dialogue(PurpleConnection *gc, const contact_info *info)
{
	qq_data *qd;
	PurpleRequestFieldGroup *group;
	PurpleRequestFields *fields;
	PurpleRequestField *field;
	modify_info_data *mid;

	/* so we only have one dialog open at a time */
	qd = (qq_data *) gc->proto_data;
	if (!qd->modifying_info) {
		qd->modifying_info = TRUE;

		fields = purple_request_fields_new();

		group = setup_field_group(fields, QQ_PRIMARY_INFORMATION);
		field = purple_request_field_string_new("uid", QQ_NUMBER, info->uid, FALSE);
		purple_request_field_group_add_field(group, field);
		purple_request_field_string_set_editable(field, FALSE);
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
		field = purple_request_field_string_new("intro", QQ_INTRO, info->intro, TRUE);
		purple_request_field_group_add_field(group, field);

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

		purple_request_fields(gc, _("Modify my information"),
				_("Modify my information"), NULL, fields,
				_("Update my information"), G_CALLBACK(modify_info_ok_cb),
				_("Cancel"), G_CALLBACK(modify_info_cancel_cb),
				purple_connection_get_account(gc), NULL, NULL,
				mid);
	}
}

/* process the reply of modify_info packet */
void qq_process_modify_info_reply(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	data[data_len] = '\0';
	if (qd->uid == atoi((gchar *) data)) {	/* return should be my uid */
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Update info ACK OK\n");
		purple_notify_info(gc, NULL, _("Your information has been updated"), NULL);
	}
}

static void _qq_send_packet_modify_face(PurpleConnection *gc, gint face_num)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	PurplePresence *presence = purple_account_get_presence(account);
	qq_data *qd = (qq_data *) gc->proto_data;
	gint offset;

	if(purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_INVISIBLE)) {
		offset = 2;
	} else if(purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_AWAY)
			|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_EXTENDED_AWAY)) {
		offset = 1;
	} else {
		offset = 0;
	}

	qd->my_icon = 3 * (face_num - 1) + offset;
	qd->modifying_face = TRUE;
	qq_send_packet_get_info(gc, qd->uid, FALSE);
}

void qq_set_buddy_icon_for_user(PurpleAccount *account, const gchar *who, const gchar *icon_num, const gchar *iconfile)
{
	gchar *data;
	gsize len;

	if (!g_file_get_contents(iconfile, &data, &len, NULL)) {
		g_return_if_reached();
	} else {
		purple_buddy_icons_set_for_user(account, who, data, len, icon_num);
	}
}

/* TODO: custom faces for QQ members and users with level >= 16 */
void qq_set_my_buddy_icon(PurpleConnection *gc, PurpleStoredImage *img)
{
	gchar *icon;
	gint icon_num;
	gint icon_len;
	PurpleAccount *account = purple_connection_get_account(gc);
	const gchar *icon_path = purple_account_get_buddy_icon_path(account);
	const gchar *buddy_icon_dir = qq_buddy_icon_dir();
	gint prefix_len = strlen(QQ_ICON_PREFIX);
	gint suffix_len = strlen(QQ_ICON_SUFFIX);
	gint dir_len = strlen(buddy_icon_dir);
	gchar *errmsg = g_strdup_printf(_("Setting custom faces is not currently supported. Please choose an image from %s."), buddy_icon_dir);
	gboolean icon_global = purple_account_get_bool(gc->account, "use-global-buddyicon", TRUE);

	if (!icon_path)
		icon_path = "";

	icon_len = strlen(icon_path) - dir_len - 1 - prefix_len - suffix_len;

	/* make sure we're using an appropriate icon */
	if (!(g_ascii_strncasecmp(icon_path, buddy_icon_dir, dir_len) == 0
				&& icon_path[dir_len] == G_DIR_SEPARATOR
				&& g_ascii_strncasecmp(icon_path + dir_len + 1, QQ_ICON_PREFIX, prefix_len) == 0
				&& g_ascii_strncasecmp(icon_path + dir_len + 1 + prefix_len + icon_len, QQ_ICON_SUFFIX, suffix_len) == 0
				&& icon_len <= 3)) {
		if (icon_global)
			purple_debug(PURPLE_DEBUG_ERROR, "QQ", "%s\n", errmsg);
		else
			purple_notify_error(gc, _("Invalid QQ Face"), errmsg, NULL);
		g_free(errmsg);
		return;
	}
	/* strip everything but number */
	icon = g_strndup(icon_path + dir_len + 1 + prefix_len, icon_len);
	icon_num = strtol(icon, NULL, 10);
	g_free(icon);
	/* ensure face number in proper range */
	if (icon_num > QQ_FACES) {
		if (icon_global)
			purple_debug(PURPLE_DEBUG_ERROR, "QQ", "%s\n", errmsg);
		else
			purple_notify_error(gc, _("Invalid QQ Face"), errmsg, NULL);
		g_free(errmsg);
		return;
	}
	g_free(errmsg);
	/* tell server my icon changed */
	_qq_send_packet_modify_face(gc, icon_num);
	/* display in blist */
	qq_set_buddy_icon_for_user(account, account->username, icon, icon_path);
}


static void _qq_update_buddy_icon(PurpleAccount *account, const gchar *name, gint face)
{
	PurpleBuddy *buddy;
	gchar *icon_num_str = face_to_icon_str(face);
	const gchar *old_icon_num = NULL;

	if ((buddy = purple_find_buddy(account, name)))
		old_icon_num = purple_buddy_icons_get_checksum_for_user(buddy);

	if (old_icon_num == NULL ||
			strcmp(icon_num_str, old_icon_num))
	{
		gchar *icon_path;

		icon_path = g_strconcat(qq_buddy_icon_dir(), G_DIR_SEPARATOR_S,
				QQ_ICON_PREFIX, icon_num_str,
				QQ_ICON_SUFFIX, NULL);

		qq_set_buddy_icon_for_user(account, name, icon_num_str, icon_path);
		g_free(icon_path);
	}
	g_free(icon_num_str);
}

/* after getting info or modify myself, refresh the buddy list accordingly */
static void qq_refresh_buddy_and_myself(contact_info *info, PurpleConnection *gc)
{
	PurpleBuddy *b;
	qq_data *qd;
	qq_buddy *q_bud;
	gchar *alias_utf8;
	gchar *purple_name;
	PurpleAccount *account = purple_connection_get_account(gc);

	qd = (qq_data *) gc->proto_data;
	purple_name = uid_to_purple_name(strtol(info->uid, NULL, 10));

	alias_utf8 = qq_to_utf8(info->nick, QQ_CHARSET_DEFAULT);
	if (qd->uid == strtol(info->uid, NULL, 10)) {	/* it is me */
		qd->my_icon = strtol(info->face, NULL, 10);
		if (alias_utf8 != NULL)
			purple_account_set_alias(account, alias_utf8);
	}
	/* update buddy list (including myself, if myself is the buddy) */
	b = purple_find_buddy(gc->account, purple_name);
	q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;
	if (q_bud != NULL) {	/* I have this buddy */
		q_bud->age = strtol(info->age, NULL, 10);
		q_bud->gender = strtol(info->gender, NULL, 10);
		q_bud->face = strtol(info->face, NULL, 10);
		if (alias_utf8 != NULL)
			q_bud->nickname = g_strdup(alias_utf8);
		qq_update_buddy_contact(gc, q_bud);
		_qq_update_buddy_icon(gc->account, purple_name, q_bud->face);
	}
	g_free(purple_name);
	g_free(alias_utf8);
}

/* process reply to get_info packet */
void qq_process_get_info_reply(guint8 *data, gint data_len, PurpleConnection *gc)
{
	gchar **segments;
	qq_info_query *query;
	qq_data *qd;
	contact_info *info;
	GList *list, *query_list;
	PurpleNotifyUserInfo *user_info;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;
	list = query_list = NULL;
	info = NULL;

	if (NULL == (segments = split_data(data, data_len, "\x1e", QQ_CONTACT_FIELDS)))
		return;

	info = (contact_info *) segments;
	if (qd->modifying_face && strtol(info->face, NULL, 10) != qd->my_icon) {
		gchar *icon = g_strdup_printf("%d", qd->my_icon);
		qd->modifying_face = FALSE;
		g_free(info->face);
		info->face = icon;
		qq_send_packet_modify_info(gc, (contact_info *)segments);
	}

	qq_refresh_buddy_and_myself(info, gc);

	query_list = qd->info_query;
	/* ensure we're processing the right query */
	while (query_list) {
		query = (qq_info_query *) query_list->data;
		if (query->uid == atoi(info->uid)) {
			if (query->show_window) {
				user_info = info_to_notify_user_info(info);
				purple_notify_userinfo(gc, info->uid, user_info, NULL, NULL);
				purple_notify_user_info_destroy(user_info);
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
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "%d info queries are freed!\n", i);
}

void qq_send_packet_get_level(PurpleConnection *gc, guint32 uid)
{
	qq_data *qd = (qq_data *) gc->proto_data;
	guint8 buf[16] = {0};
	gint bytes = 0;

	bytes += qq_put8(buf + bytes, 0x00);
	bytes += qq_put32(buf + bytes, uid);

	qd = (qq_data *) gc->proto_data;
	qq_send_cmd(qd, QQ_CMD_GET_LEVEL, buf, bytes);
}

void qq_send_packet_get_buddies_levels(PurpleConnection *gc)
{
	guint8 *buf;
	guint16 size;
	qq_buddy *q_bud;
	qq_data *qd = (qq_data *) gc->proto_data;
	GList *node = qd->buddies;
	gint bytes = 0;

	if ( qd->buddies == NULL) {
		return;
	}
	/* server only sends back levels for online buddies, no point
	 * in asking for anyone else */
	size = 4 * g_list_length(qd->buddies) + 1;
	buf = g_newa(guint8, size);
	bytes += qq_put8(buf + bytes, 0x00);
	
	while (NULL != node) {
		q_bud = (qq_buddy *) node->data;
		if (NULL != q_bud) {
			bytes += qq_put32(buf + bytes, q_bud->uid);
		}
		node = node->next;
	}
	qq_send_cmd(qd, QQ_CMD_GET_LEVEL, buf, size);
}

void qq_process_get_level_reply(guint8 *decr_buf, gint decr_len, PurpleConnection *gc)
{
	guint32 uid, onlineTime;
	guint16 level, timeRemainder;
	gchar *purple_name;
	PurpleBuddy *b;
	qq_buddy *q_bud;
	gint i;
	PurpleAccount *account = purple_connection_get_account(gc);
	qq_data *qd = (qq_data *) gc->proto_data;
	gint bytes = 0;

	decr_len--; 
	if (decr_len % 12 != 0) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"Get levels list of abnormal length. Truncating last %d bytes.\n", decr_len % 12);
		decr_len -= (decr_len % 12);
	}

	bytes += 1;
	/* this byte seems random */
	/*
	   purple_debug(PURPLE_DEBUG_INFO, "QQ", "Byte one of get_level packet: %d\n", buf[0]);
	   */
	for (i = 0; i < decr_len; i += 12) {
		bytes += qq_get32(&uid, decr_buf + bytes);
		bytes += qq_get32(&onlineTime, decr_buf + bytes);
		bytes += qq_get16(&level, decr_buf + bytes);
		bytes += qq_get16(&timeRemainder, decr_buf + bytes);
		purple_debug(PURPLE_DEBUG_INFO, "QQ_LEVEL", 
				"%d, tmOnline: %d, level: %d, tmRemainder: %d\n", 
				uid, onlineTime, level, timeRemainder);
		if (uid == qd->uid) {
			qd->my_level = level;
			purple_debug(PURPLE_DEBUG_WARNING, "QQ", "Got my levels as %d\n", qd->my_level);
			continue;
		}

		purple_name = uid_to_purple_name(uid);
		if (purple_name == NULL) {
			continue;
		}
		
		b = purple_find_buddy(account, purple_name);
		g_free(purple_name);

		q_bud = NULL;
		if (b != NULL) {
			q_bud = (qq_buddy *) b->proto_data;
		}
		
		if (q_bud == NULL) {
			purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
					"Got levels of %d not in my buddy list\n", uid);
			continue;
		}

		q_bud->onlineTime = onlineTime;
		q_bud->level = level;
		q_bud->timeRemainder = timeRemainder;
	}
}

