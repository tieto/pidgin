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

/* Below is all of the information necessary to reconstruct the various
 * information fields that one can set in the official client. When we need
 * to know about a specific field (e.g., should "city" be a choice
 * or text field?), we can simply look it up from the template. Note that
 * there are a number of unidentified fields. */
typedef struct _info_field {
	gchar *title;
	gchar *id;		/* used by gaim_request fields */
	gint pos;
	gchar *group;
	gint group_pos;		/* for display order in the UI */
	gint choice;		/* indicates which character array contains the choices */
	gboolean customizable;	/* whether a user can enter any text as a value, regardless of choice arrays */
	gchar *value;
} info_field;

static const info_field info_template_data[] = {
	{ N_("User ID"), "uid", 		0, QQ_MAIN_INFO, 0, QQ_NO_CHOICE, TRUE, NULL },	
	{ N_("Nickname"), "nick",		1, QQ_MAIN_INFO, 1, QQ_NO_CHOICE, TRUE, NULL },	
	{ N_("Country/Region"), "country",	2, QQ_MAIN_INFO, 5, QQ_COUNTRY, TRUE, NULL },
	{ N_("Province/State"), "province",	3, QQ_MAIN_INFO, 6, QQ_PROVINCE, TRUE, NULL },
	{ N_("Zipcode"), "zipcode",		4, QQ_EXTRA_INFO, 7, QQ_NO_CHOICE, TRUE, NULL },
	{ N_("Address"), "address",		5, QQ_EXTRA_INFO, 6, QQ_NO_CHOICE, TRUE, NULL },
	{ N_("Phone Number"), "tel",		6, QQ_EXTRA_INFO, 9, QQ_NO_CHOICE, TRUE, NULL },
	{ N_("Age"), "age",			7, QQ_MAIN_INFO, 3, QQ_NO_CHOICE, TRUE, NULL },
	{ N_("Gender"), "gender",		8, QQ_MAIN_INFO, 4, QQ_GENDER, FALSE, NULL },
	{ N_("Name"), "name",			9, QQ_MAIN_INFO, 2, QQ_NO_CHOICE, TRUE, NULL },
	{ N_("Email"), "email",			10, QQ_EXTRA_INFO, 5, QQ_NO_CHOICE, TRUE, NULL },
	{ "pager_sn", "pager_sn",		11, QQ_MISC, 0, QQ_NO_CHOICE, TRUE, NULL },
	{ "pager_num", "pager_num",		12, QQ_MISC, 1, QQ_NO_CHOICE, TRUE, NULL },
	{ "pager_sp", "pager_sp",		13, QQ_MISC, 2, QQ_NO_CHOICE, TRUE, NULL },
	{ "pager_base_num", "pager_base_num",	14, QQ_MISC, 3, QQ_NO_CHOICE, TRUE, NULL },
	{ "pager_type", "pager_type",		15, QQ_MISC, 4, QQ_NO_CHOICE, TRUE, NULL },
	{ N_("Occupation"), "occupation",	16, QQ_EXTRA_INFO, 1, QQ_OCCUPATION, TRUE, NULL },
	{ N_("Homepage"), "homepage",		17, QQ_EXTRA_INFO, 10, QQ_NO_CHOICE, TRUE, NULL },
	{ "auth_type", "auth_type",		18, QQ_MISC, 5, QQ_NO_CHOICE, TRUE, NULL },
	{ "unknown1", "unknown1",		19, QQ_MISC, 6, QQ_NO_CHOICE, TRUE, NULL },
	{ "unknown2", "unknown2",		20, QQ_MISC, 7, QQ_NO_CHOICE, TRUE, NULL },
	{ "face", "face",			21, QQ_MISC, 8, QQ_NO_CHOICE, TRUE, NULL },
	{ N_("Cellphone Number"), "hp_num",	22, QQ_EXTRA_INFO, 8, QQ_NO_CHOICE, TRUE, NULL },
	{ "hp_type", "hp_type",			23, QQ_MISC, 9, QQ_NO_CHOICE, TRUE, NULL },
	{ N_("Personal Introduction"), "intro",	24, QQ_PERSONAL_INTRO, 0, QQ_NO_CHOICE, TRUE, NULL },
	{ N_("City"), "city",			25, QQ_MAIN_INFO, 7, QQ_NO_CHOICE, TRUE, NULL },
	{ "unknown3", "unknown3",		26, QQ_MISC, 10, QQ_NO_CHOICE, TRUE, NULL },
	{ "unknown4", "unknown4",		27, QQ_MISC, 11, QQ_NO_CHOICE, TRUE, NULL },
	{ "unknown5", "unknown5",		28, QQ_MISC, 12, QQ_NO_CHOICE, TRUE, NULL },
	{ "is_open_hp",	"is_open_hp",		29, QQ_MISC, 13, QQ_NO_CHOICE, TRUE, NULL },
	{ "is_open_contact", "is_open_contact",	30, QQ_MISC, 14, QQ_NO_CHOICE, TRUE, NULL },
	{ N_("College"), "college",		31, QQ_EXTRA_INFO, 4, QQ_NO_CHOICE, TRUE, NULL },
	{ N_("Horoscope Symbol"), "horoscope",	32, QQ_EXTRA_INFO, 0, QQ_HOROSCOPE, FALSE, NULL },
	{ N_("Zodiac Symbol"), "zodiac",	33, QQ_EXTRA_INFO, 2, QQ_ZODIAC, FALSE, NULL },
	{ N_("Blood Type"), "blood",		34, QQ_EXTRA_INFO, 3, QQ_BLOOD, FALSE, NULL },
	{ "qq_show", "qq_show",			35, QQ_MISC, 15, QQ_NO_CHOICE, TRUE, NULL },
	{ "unknown6", "unknown6",		36, QQ_MISC, 16, QQ_NO_CHOICE, TRUE, NULL },
	{ NULL,	NULL, 0, NULL, 0, 0, 0, NULL	}
};

/* TODO: translate these arrays to their English equivalents
 * and move these characters to the zh_CN po file */
static const gchar *horoscope_names[] = {
	"-", "水瓶座", "双鱼座", "牡羊座", "金牛座",
	"双子座", "巨蟹座", "狮子座", "处女座", "天秤座",
        "天蝎座", "射手座", "魔羯座", NULL
};

static const gchar *zodiac_names[] = {
	"-", "鼠", "牛", "虎", "兔",
	"龙", "蛇", "马", "羊", "猴",
	"鸡", "狗", "猪", NULL
};

static const gchar *blood_types[] = {
	"-", N_("A"), N_("B"), N_("O"), N_("AB"), N_("Other"), NULL
};

static const gchar *genders[] = {
	N_("Male"),
	N_("Female"),
	NULL
};

static const gchar *country_names[] = {
        "中国", "中国香港", "中国澳门", "中国台湾",
        "新加坡", "马来西亚", "美国", NULL
};

static const gchar *province_names[] = {
        "北京", "天津", "上海", "重庆", "香港",
        "河北", "山西", "内蒙古", "辽宁", "吉林",
        "黑龙江", "江西", "浙江", "江苏", "安徽",
        "福建", "山东", "河南", "湖北", "湖南",
        "广东", "广西", "海南", "四川", "贵州",
        "云南", "西藏", "陕西", "甘肃", "宁夏",
        "青海", "新疆", "台湾", "澳门", NULL
};

static const gchar *occupation_names[] = {
        "全职", "兼职", "制造业", "商业", "失业中",
        "学生", "工程师", "政府部门", "教育业", "服务行业",
        "老板", "计算机业", "退休", "金融业",
        "销售/广告/市场", NULL
};

static const gint choice_sizes[] = { 0, 13, 13, 6, 2, 7, 34, 15 };


static const gchar *info_group_headers[] = {
       QQ_MAIN_INFO,
       QQ_EXTRA_INFO,
       QQ_PERSONAL_INTRO,
       QQ_MISC
};

static const gchar **choices[] = {
        NULL,
        horoscope_names,
        zodiac_names,
        blood_types,
        genders,
        country_names,
        province_names,
        occupation_names
};

/************************ info and info_field methods ************************/

/* Given an id, return the template for that field.
 * Returns NULL if the id is not found. */
static const info_field *info_field_get_template(const gchar *id)
{
	const info_field *cur_field;
	const gchar *cur_id;
	
	cur_field = info_template_data;
	cur_id = cur_field->id;
	while(cur_id != NULL) {
		if (g_ascii_strcasecmp(cur_id, id) == 0) 
			return cur_field;
		cur_field++;
		cur_id = cur_field->id;
	}
	gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Info field with id %s not found!", id);
	return NULL;
}

/* info_fields are compared by their group positions */
static gint info_field_compare(gconstpointer a, gconstpointer b)
{
	return ((info_field *) a)->group_pos - ((info_field *) b)->group_pos;
}

static void info_field_free(info_field *i)
{
	g_free(i->value);
	g_free(i);
}

/* Parses the info_template_data above and returns a newly-allocated list
 * containing the desired fields from segments. This list is ordered by
 * group_pos. */
static GList *info_get_group(const gchar **info, const gchar *group_name)
{
	const info_field *cur;
       	info_field *entry;
	GList *group = NULL;

	cur = info_template_data;
	while (cur->id != NULL) {
		if (g_ascii_strcasecmp(group_name, cur->group) == 0) {
			entry = g_memdup(cur, sizeof(info_field));
			entry->value = g_strdup(info[entry->pos]);
			group = g_list_insert_sorted(group, entry, info_field_compare);
		}
		cur++;
	}

	return group;
}

/* Determines if the given text value and choice group require
 * a lookup from the choice arrays. */
static gboolean is_valid_index(gchar *value, gint choice)
{
	gint len, i;

	if (choice == 0) return FALSE;
	len = strlen(value);
	/* the server sends us an ascii index and none of the arrays has more than 99
	 * elements */
	if (len > 3 || len == 0) return FALSE;
	for (i = 0; i < len; i++)
		if (!g_ascii_isdigit(value[i])) 
			return FALSE;
	i = atoi(value);
	if (i < 0 || i >= choice_sizes[choice]) 
		return FALSE; 
	return TRUE;
}

/* formats a field for printing */
static void append_field_to_str(gpointer field, gpointer str)
{
	info_field *f;
	gint choice;
	gboolean valid_index;
	gchar *value;

	f = (info_field *) field;
	choice = f->choice;
	valid_index = is_valid_index(f->value, choice);
	if (choice && valid_index) value = g_strdup(choices[choice][atoi(f->value)]);
	else value = qq_to_utf8(f->value, QQ_CHARSET_DEFAULT);
	g_string_append_printf((GString *) str, "<b>%s:</b> %s<br />",
			f->title, value);
	g_free(value);
	info_field_free(f);
}

/* formats a group of information for printing */
static void append_group_to_str(GString *str, const gchar *group_name, const gchar **info)
{
	GList *group;

	group = info_get_group(info, group_name);
	g_string_append_printf(str, "<b>%s</b><br /><br />", (*(info_field *) group->data).group);
	g_list_foreach(group, append_field_to_str, str);
	g_list_free(group);
	g_string_append_printf(str, "<br />");
}

/* Takes a contact_info struct and outputs the appropriate fields in
 * a printable format for our upcoming call to gaim_notify_userinfo. */
static GString *info_to_str(const gchar **info)
{
	GString *info_text;

	info_text = g_string_new("");
	append_group_to_str(info_text, QQ_MAIN_INFO, info);
	append_group_to_str(info_text, QQ_EXTRA_INFO, info);
	append_group_to_str(info_text, QQ_PERSONAL_INTRO, info);
	/* append_group_to_str(info_text, QQ_MISC, info); */

	return info_text;
}

/************************ packets and UI management **************************/

/* send a packet to get detailed information of uid */
void qq_send_packet_get_info(GaimConnection *gc, guint32 uid, gboolean show_window)
{
	qq_data *qd;
	gchar *uid_str;
	qq_info_query *query;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL && uid != 0);

	qd = (qq_data *) gc->proto_data;
	uid_str = g_strdup_printf("%d", uid);
	qq_send_cmd(gc, QQ_CMD_GET_USER_INFO, TRUE, 0, TRUE, (guint8 *) uid_str, strlen(uid_str));

	query = g_new0(qq_info_query, 1);
	query->uid = uid;
	query->show_window = show_window;
	query->modify_info = FALSE;
	qd->info_query = g_list_append(qd->info_query, query);

	g_free(uid_str);
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
void qq_send_packet_modify_info(GaimConnection *gc, contact_info *info)
{
	gchar *info_field[QQ_CONTACT_FIELDS];
	gint i;
	guint8 *raw_data, *cursor, bar;

	g_return_if_fail(gc != NULL && info != NULL);

	bar = 0x1f;
	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 128);
	cursor = raw_data;

	g_memmove(info_field, info, sizeof(gchar *) * QQ_CONTACT_FIELDS);

	create_packet_b(raw_data, &cursor, bar);

	/* important!, skip the first uid entry */
	for (i = 1; i < QQ_CONTACT_FIELDS; i++) {
		create_packet_b(raw_data, &cursor, bar);
		create_packet_data(raw_data, &cursor, (guint8 *) info_field[i], strlen(info_field[i]));
	}
	create_packet_b(raw_data, &cursor, bar);

	qq_send_cmd(gc, QQ_CMD_UPDATE_INFO, TRUE, 0, TRUE, raw_data, cursor - raw_data);

}

static void modify_info_cancel_cb(modify_info_data *mid)
{
	qq_data *qd;

	qd = (qq_data *) mid->gc->proto_data;
	qd->modifying_info = FALSE;

	g_list_free(mid->misc);
	g_free(mid);
}

/* Runs through all of the fields in the modify info UI and puts
 * their values into the outgoing packet. */
static void parse_field(gpointer field, gpointer outgoing_info)
{
	GaimRequestField *f;
	gchar **segments, *value;
	const info_field *ft;
	const gchar *id;

	f = (GaimRequestField *) field;
	segments = (gchar **) outgoing_info;
	id = gaim_request_field_get_id(f);
	ft = info_field_get_template(id);
	if (ft->choice && !ft->customizable) {
		value = g_strdup_printf("%d", gaim_request_field_choice_get_value(f));
	} else {
		value = (gchar *) gaim_request_field_string_get_value(f);
		if (value == NULL) 
			value = g_strdup("-");
		else 
			value = utf8_to_qq(value, QQ_CHARSET_DEFAULT);
	}
	segments[ft->pos] = value;
}

/* dumps the uneditable information straight into the outgoing packet */
static void parse_misc_field(gpointer field, gpointer outgoing_info)
{
	info_field *f;
	gchar **segments;

	f = (info_field *) field;
	segments = (gchar **) outgoing_info;
	segments[f->pos] = g_strdup(f->value);
	info_field_free(f);
}

/* Runs through all of the information fields and copies them into an
 * outgoing packet, then sends that packet. */
static void modify_info_ok_cb(modify_info_data *mid, GaimRequestFields *fields)
{
	GaimConnection *gc;
	qq_data *qd;
	GList *list,  *groups, *group_node;
	gchar *info_field[QQ_CONTACT_FIELDS];
	contact_info *info;
	gint i;

	gc = mid->gc;
	qd = (qq_data *) gc->proto_data;
	qd->modifying_info = FALSE;
	list = mid->misc;
	g_list_foreach(list, parse_misc_field, info_field);
	g_list_free(list);
	groups = gaim_request_fields_get_groups(fields);
	while(groups) {
		group_node = groups;
		list = gaim_request_field_group_get_fields(group_node->data);
		g_list_foreach(list, parse_field, info_field);
		groups = g_list_remove_link(groups, group_node);
	}
	info = (contact_info *) info_field;

	qq_send_packet_modify_info(gc, info);
	g_free(mid);
	for (i = 0; i < QQ_CONTACT_FIELDS; i++)
		g_free(info_field[i]);
}

/* Sets up the display for one group of information. This includes
 * managing which fields in the UI should be textfields and
 * which choices, and also mapping ints to choice values when appropriate. */
static void setup_group(gpointer field, gpointer group)
{
	info_field *f;
	GaimRequestFieldGroup *g;
	GaimRequestField *rf;
	gint choice, index, j;
	gboolean customizable, valid_index, multiline;
	gchar *id, *value;

	f = (info_field *) field;
	g = (GaimRequestFieldGroup *) group;
	choice = f->choice;
	customizable = f->customizable;
	id = f->id;
	valid_index = TRUE;

	if (!choice || customizable) {
		valid_index = is_valid_index(f->value, choice);
		multiline = id == "intro";
		if (valid_index) {
			index = atoi(f->value);
			value = (gchar *) choices[choice][index];
		} else {
			value = qq_to_utf8(f->value, QQ_CHARSET_DEFAULT);
		}
		rf = gaim_request_field_string_new(id, f->title, value, multiline);
	} else {
		index = atoi(f->value);
		value = (gchar *) choices[choice][index];
		rf = gaim_request_field_choice_new(id, f->title, index);
		j = 0;
		while(choices[choice][j] != NULL)
			gaim_request_field_choice_add(rf, choices[choice][j++]);
	}
	gaim_request_field_group_add_field(g, rf);
	if (!valid_index) 
		g_free(value);
	info_field_free(f);
}

/* Takes the info returned by a get_info packet for the user and sets up
 * a form using those values and the info_template. */
static void create_modify_info_dialogue(GaimConnection *gc, const gchar **info)
{
	qq_data *qd;
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;
	GList *group_list;
	modify_info_data *mid;
	gint i;

	/* so we only have one dialog open at a time */
	qd = (qq_data *) gc->proto_data;
	if (!qd->modifying_info) {
		qd->modifying_info = TRUE;

		fields = gaim_request_fields_new();
	
		/* we only care about the first 3 groups, not the miscellaneous stuff */
		for (i = 0; i < 3; i++) {
			group = gaim_request_field_group_new(info_group_headers[i]);
			gaim_request_fields_add_group(fields, group);
			group_list = info_get_group(info, info_group_headers[i]);
			g_list_foreach(group_list, setup_group, group);
			g_list_free(group_list);
		}

		field = gaim_request_fields_get_field(fields, "uid");
		gaim_request_field_string_set_editable(field, FALSE);

		mid = g_new0(modify_info_data, 1);
		mid->gc = gc;
		mid->misc = info_get_group(info, info_group_headers[3]);
	
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
			qq_send_packet_modify_info(gc, info);
			g_free(icon);
		}

		qq_refresh_buddy_and_myself(info, gc);

		query_list = qd->info_query;
		/* ensure we're processing the right query */
		while (query_list) {
			query = (qq_info_query *) query_list->data;
			if (query->uid == atoi(info->uid)) {
				if (query->show_window) {
					info_text = info_to_str((const gchar **) segments);
					gaim_notify_userinfo(gc, info->uid, info_text->str, NULL, NULL);
					g_string_free(info_text, TRUE);
				} else if (query->modify_info) {
					create_modify_info_dialogue(gc, (const gchar **) segments);
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
