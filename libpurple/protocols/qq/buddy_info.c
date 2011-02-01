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
#include "connection.h"

#include "utils.h"
#include "packet_parse.h"
#include "buddy_list.h"
#include "buddy_info.h"
#include "char_conv.h"
#include "im.h"
#include "qq_define.h"
#include "qq_base.h"
#include "qq_network.h"

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

#define QQ_PUBLISH_SIZE 3
static const gchar *publish_types[] = {
	N_("Visible"), N_("Friend Only"), N_("Private")
};

#define QQ_GENDER_SIZE 3
static const gchar *genders[] = {
	N_("Private"),
	N_("Male"),
	N_("Female"),
};

static const gchar *genders_zh[] = {
	"-",
	"\xc4\xd0",
	"\xc5\xae",
};

#define QQ_FACES	    134
#define QQ_ICON_PREFIX "qq_"
#define QQ_ICON_SUFFIX ".png"

enum {
	QQ_INFO_UID = 0, QQ_INFO_NICK, QQ_INFO_COUNTRY, QQ_INFO_PROVINCE, QQ_INFO_ZIPCODE,
	QQ_INFO_ADDR, QQ_INFO_TEL, QQ_INFO_AGE, QQ_INFO_GENDER, QQ_INFO_NAME, QQ_INFO_EMAIL,
	QQ_INFO_PG_SN, QQ_INFO_PG_NUM, QQ_INFO_PG_SP, QQ_INFO_PG_BASE_NUM, QQ_INFO_PG_TYPE,
	QQ_INFO_OCCU, QQ_INFO_HOME_PAGE, QQ_INFO_AUTH_TYPE, QQ_INFO_UNKNOW1, QQ_INFO_UNKNOW2,
	QQ_INFO_FACE, QQ_INFO_MOBILE, QQ_INFO_MOBILE_TYPE, QQ_INFO_INTRO, QQ_INFO_CITY,
	QQ_INFO_UNKNOW3, QQ_INFO_UNKNOW4, QQ_INFO_UNKNOW5,
	QQ_INFO_IS_PUB_MOBILE, QQ_INFO_IS_PUB_CONTACT, QQ_INFO_COLLEGE, QQ_INFO_HOROSCOPE,
	QQ_INFO_ZODIAC, QQ_INFO_BLOOD, QQ_INFO_SHOW, QQ_INFO_UNKNOW6,
	QQ_INFO_LAST_2007, QQ_INFO_LAST
};

enum {
	QQ_FIELD_UNUSED = 0, QQ_FIELD_BASE, QQ_FIELD_EXT, QQ_FIELD_CONTACT, QQ_FIELD_ADDR
};

enum {
	QQ_FIELD_LABEL = 0, QQ_FIELD_STRING, QQ_FIELD_MULTI, QQ_FIELD_BOOL, QQ_FIELD_CHOICE
};

typedef struct {
	int iclass;
	int type;
	char *id;
	char *text;
	const gchar **choice;
	int choice_size;
} QQ_FIELD_INFO;

static const QQ_FIELD_INFO field_infos[] = {
	{ QQ_FIELD_BASE, 	QQ_FIELD_STRING, "uid", 	N_("QQ Number"), NULL, 0 },
	{ QQ_FIELD_BASE, 	QQ_FIELD_STRING, "nick", 	N_("Nickname"), NULL, 0 },
	{ QQ_FIELD_ADDR, 	QQ_FIELD_STRING, "country", 	N_("Country/Region"), NULL, 0 },
	{ QQ_FIELD_ADDR, 	QQ_FIELD_STRING, "province", 	N_("Province/State"), NULL, 0 },
	{ QQ_FIELD_ADDR, 	QQ_FIELD_STRING, "zipcode", 	N_("Zipcode"), NULL, 0 },
	{ QQ_FIELD_ADDR, 	QQ_FIELD_STRING, "address", 	N_("Address"), NULL, 0 },
	{ QQ_FIELD_CONTACT, 	QQ_FIELD_STRING, "tel", 	N_("Phone Number"), NULL, 0 },
	{ QQ_FIELD_BASE, 	QQ_FIELD_STRING, "age", 	N_("Age"), NULL, 0 },
	{ QQ_FIELD_BASE, 	QQ_FIELD_CHOICE, "gender", 	N_("Gender"), genders, QQ_GENDER_SIZE },
	{ QQ_FIELD_BASE, 	QQ_FIELD_STRING, "name", 	N_("Name"), NULL, 0 },
	{ QQ_FIELD_CONTACT, 	QQ_FIELD_STRING, "email", 	N_("Email"), NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "pg_sn",	"Pager Serial Num", NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "pg_num",	"Pager Num", NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "pg_sp",	"Pager Serivce Provider", NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "pg_sta",	"Pager Station Num", NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "pg_type",	"Pager Type", NULL, 0 },
	{ QQ_FIELD_BASE, 	QQ_FIELD_STRING, "occupation", 	N_("Occupation"), NULL, 0 },
	{ QQ_FIELD_CONTACT, 	QQ_FIELD_STRING, "homepage", 	N_("Homepage"), NULL, 0 },
	{ QQ_FIELD_BASE, 	QQ_FIELD_BOOL, 	"auth", 	N_("Authorize adding"), NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "unknow1",	"Unknow1", NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "unknow2",	"Unknow2", NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "face",	"Face", NULL, 0 },
	{ QQ_FIELD_CONTACT, 	QQ_FIELD_STRING, "mobile",	N_("Cellphone Number"), NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "mobile_type", "Cellphone Type", NULL, 0 },
	{ QQ_FIELD_BASE, 	QQ_FIELD_MULTI,  "intro", 	N_("Personal Introduction"), NULL, 0 },
	{ QQ_FIELD_ADDR, 	QQ_FIELD_STRING, "city",	N_("City/Area"), NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "unknow3",	"Unknow3", NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "unknow4",	"Unknow4", NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "unknow5",	"Unknow5", NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_CHOICE, "pub_mobile",	N_("Publish Mobile"), publish_types, QQ_PUBLISH_SIZE },
	{ QQ_FIELD_CONTACT, 	QQ_FIELD_CHOICE, "pub_contact",	N_("Publish Contact"), publish_types, QQ_PUBLISH_SIZE },
	{ QQ_FIELD_EXT, 	QQ_FIELD_STRING, "college",	N_("College"), NULL, 0 },
	{ QQ_FIELD_EXT, 	QQ_FIELD_CHOICE, "horoscope",	N_("Horoscope"), horoscope_names, QQ_HOROSCOPE_SIZE },
	{ QQ_FIELD_EXT, 	QQ_FIELD_CHOICE, "zodiac",	N_("Zodiac"), zodiac_names, QQ_ZODIAC_SIZE },
	{ QQ_FIELD_EXT, 	QQ_FIELD_CHOICE, "blood",	N_("Blood"), blood_types, QQ_BLOOD_SIZE },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "qq_show",	"QQ Show", NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "unknow6",	"Unknow6", NULL, 0 },
	{ QQ_FIELD_UNUSED, 	QQ_FIELD_STRING, "LAST_2005",	"LAST_2005", NULL, 0 }
};

typedef struct _modify_info_request {
	PurpleConnection *gc;
	int iclass;
	gchar **segments;
} modify_info_request;

#ifdef DEBUG
static void info_debug(gchar **segments)
{
#if 0
	int index;
	gchar *utf8_str;
	for (index = 0; segments[index] != NULL && index < QQ_INFO_LAST; index++) {
		if (field_infos[index].type == QQ_FIELD_STRING
				|| field_infos[index].type == QQ_FIELD_LABEL
				|| field_infos[index].type == QQ_FIELD_MULTI
				|| index == QQ_INFO_GENDER)  {
			utf8_str = qq_to_utf8(segments[index], QQ_CHARSET_DEFAULT);
			purple_debug_info("QQ_BUDDY_INFO", "%s: %s\n", field_infos[index].text, utf8_str);
			g_free(utf8_str);
			continue;
		}
		purple_debug_info("QQ_BUDDY_INFO", "%s: %s\n", field_infos[index].text, segments[index]);
	}
#endif
}
#endif

static void info_display_only(PurpleConnection *gc, gchar **segments)
{
	PurpleNotifyUserInfo *user_info;
	gchar *utf8_value;
	int index;
	int choice_num;

	user_info = purple_notify_user_info_new();

	for (index = 1; segments[index] != NULL && index < QQ_INFO_LAST; index++) {
		if (field_infos[index].iclass == QQ_FIELD_UNUSED) {
			continue;
		}
		switch (field_infos[index].type) {
			case QQ_FIELD_BOOL:
				purple_notify_user_info_add_pair(user_info, _(field_infos[index].text),
					strtol(segments[index], NULL, 10) ? _("True") : _("False"));
				break;
			case QQ_FIELD_CHOICE:
				choice_num = strtol(segments[index], NULL, 10);
				if (choice_num < 0 || choice_num >= field_infos[index].choice_size) {
					choice_num = 0;
				}

				purple_notify_user_info_add_pair(user_info, _(field_infos[index].text), field_infos[index].choice[choice_num]);
				break;
			case QQ_FIELD_LABEL:
			case QQ_FIELD_STRING:
			case QQ_FIELD_MULTI:
			default:
				if (strlen(segments[index]) != 0) {
					utf8_value = qq_to_utf8(segments[index], QQ_CHARSET_DEFAULT);
					purple_notify_user_info_add_pair(user_info, _(field_infos[index].text), utf8_value);
					g_free(utf8_value);
				}
				break;
		}
	}

	purple_notify_userinfo(gc, segments[0], user_info, NULL, NULL);

	purple_notify_user_info_destroy(user_info);
	g_strfreev(segments);
}

void qq_request_buddy_info(PurpleConnection *gc, guint32 uid,
		guint32 update_class, int action)
{
	gchar raw_data[16] = {0};

	g_return_if_fail(uid != 0);

	g_snprintf(raw_data, sizeof(raw_data), "%u", uid);
	qq_send_cmd_mess(gc, QQ_CMD_GET_BUDDY_INFO, (guint8 *) raw_data, strlen(raw_data),
			update_class, action);
}

/* send packet to modify personal information */
static void request_change_info(PurpleConnection *gc, gchar **segments)
{
	gint bytes = 0;
	guint8 raw_data[MAX_PACKET_SIZE - 128] = {0};
	guint8 bar;
	gchar *join;

	g_return_if_fail(segments != NULL);

	bar = 0x1f;

	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_put8(raw_data + bytes, bar);

	/* important! skip the first uid entry */
	join = g_strjoinv("\x1f", segments + 1);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)join, strlen(join));
	g_free(join);

	bytes += qq_put8(raw_data + bytes, bar);

	/* qq_show_packet("request_modify_info", raw_data, bytes); */
	qq_send_cmd(gc, QQ_CMD_UPDATE_INFO, raw_data, bytes);
}

static void info_modify_cancel_cb(modify_info_request *info_request)
{
	g_strfreev(info_request->segments);
	g_free(info_request);
}

/* parse fields and send info packet */
static void info_modify_ok_cb(modify_info_request *info_request, PurpleRequestFields *fields)
{
	PurpleConnection *gc;
	gchar **segments;
	int index;
	const char *utf8_str;
	gchar *value;
	int choice_num;

	gc = info_request->gc;
	g_return_if_fail(gc != NULL);
	segments = info_request->segments;
	g_return_if_fail(segments != NULL);

	for (index = 1; segments[index] != NULL && index < QQ_INFO_LAST; index++) {
		if (field_infos[index].iclass == QQ_FIELD_UNUSED) {
			continue;
		}
		if (!purple_request_fields_exists(fields, field_infos[index].id)) {
			continue;
		}
		switch (field_infos[index].type) {
			case QQ_FIELD_BOOL:
				value = purple_request_fields_get_bool(fields, field_infos[index].id)
						? g_strdup("1") : g_strdup("0");
				g_free(segments[index]);
				segments[index] = value;
				break;
			case QQ_FIELD_CHOICE:
				choice_num = purple_request_fields_get_choice(fields, field_infos[index].id);
				if (choice_num < 0 || choice_num >= field_infos[index].choice_size)	choice_num = 0;

				if (index == QQ_INFO_GENDER) {
					/* QQ Server only recept gender in Chinese */
					value = g_strdup(genders_zh[choice_num]);
				} else {
					value = g_strdup_printf("%d", choice_num);
				}
				g_free(segments[index]);
				segments[index] = value;
				break;
			case QQ_FIELD_LABEL:
			case QQ_FIELD_STRING:
			case QQ_FIELD_MULTI:
			default:
				utf8_str = purple_request_fields_get_string(fields, field_infos[index].id);
				if (utf8_str == NULL) {
					value = g_strdup("-");
				} else {
					value = utf8_to_qq(utf8_str, QQ_CHARSET_DEFAULT);
					if (value == NULL) value = g_strdup("-");
				}
				g_free(segments[index]);
				segments[index] = value;
				break;
		}
	}
	request_change_info(gc, segments);

	g_strfreev(segments);
	g_free(info_request);
}

static void field_request_new(PurpleRequestFieldGroup *group, gint index, gchar **segments)
{
	PurpleRequestField *field;
	gchar *utf8_value;
	int choice_num;
	int i;

	g_return_if_fail(index >=0 && segments[index] != NULL && index < QQ_INFO_LAST);

	switch (field_infos[index].type) {
		case QQ_FIELD_STRING:
		case QQ_FIELD_MULTI:
			utf8_value = qq_to_utf8(segments[index], QQ_CHARSET_DEFAULT);
			if (field_infos[index].type == QQ_FIELD_STRING) {
				field = purple_request_field_string_new(
					field_infos[index].id, _(field_infos[index].text), utf8_value, FALSE);
			} else {
				field = purple_request_field_string_new(
					field_infos[index].id, _(field_infos[index].text), utf8_value, TRUE);
			}
			purple_request_field_group_add_field(group, field);
			g_free(utf8_value);
			break;
		case QQ_FIELD_BOOL:
			field = purple_request_field_bool_new(
				field_infos[index].id, _(field_infos[index].text),
				strtol(segments[index], NULL, 10) ? TRUE : FALSE);
			purple_request_field_group_add_field(group, field);
			break;
		case QQ_FIELD_CHOICE:
			choice_num = strtol(segments[index], NULL, 10);
			if (choice_num < 0 || choice_num >= field_infos[index].choice_size)	choice_num = 0;

			if (index == QQ_INFO_GENDER && strlen(segments[index]) != 0) {
				for (i = 0; i < QQ_GENDER_SIZE; i++) {
					if (strcmp(segments[index], genders_zh[i]) == 0) {
						choice_num = i;
					}
				}
			}
			field = purple_request_field_choice_new(
				field_infos[index].id, _(field_infos[index].text), choice_num);
			for (i = 0; i < field_infos[index].choice_size; i++) {
				purple_request_field_choice_add(field, field_infos[index].choice[i]);
			}
			purple_request_field_group_add_field(group, field);
			break;
		case QQ_FIELD_LABEL:
		default:
			field = purple_request_field_label_new(field_infos[index].id, segments[index]);
			purple_request_field_group_add_field(group, field);
			break;
	}
}

static void info_modify_dialogue(PurpleConnection *gc, gchar **segments, int iclass)
{
	PurpleRequestFieldGroup *group;
	PurpleRequestFields *fields;
	modify_info_request *info_request;
	gchar *utf8_title, *utf8_prim;
	int index;

	/* Keep one dialog once a time */
	purple_request_close_with_handle(gc);

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	for (index = 1; segments[index] != NULL && index < QQ_INFO_LAST; index++) {
		if (field_infos[index].iclass != iclass) {
			continue;
		}
		field_request_new(group, index, segments);
	}

	switch (iclass) {
		case QQ_FIELD_CONTACT:
			utf8_title = g_strdup(_("Modify Contact"));
			utf8_prim = g_strdup_printf("%s for %s", _("Modify Contact"), segments[0]);
			break;
		case QQ_FIELD_ADDR:
			utf8_title = g_strdup(_("Modify Address"));
			utf8_prim = g_strdup_printf("%s for %s", _("Modify Address"), segments[0]);
			break;
		case QQ_FIELD_EXT:
			utf8_title = g_strdup(_("Modify Extended Information"));
			utf8_prim = g_strdup_printf("%s for %s", _("Modify Extended Information"), segments[0]);
			break;
		case QQ_FIELD_BASE:
		default:
			utf8_title = g_strdup(_("Modify Information"));
			utf8_prim = g_strdup_printf("%s for %s", _("Modify Information"), segments[0]);
			break;
	}

	info_request = g_new0(modify_info_request, 1);
	info_request->gc = gc;
	info_request->iclass = iclass;
	info_request->segments = segments;

	purple_request_fields(gc,
			utf8_title,
			utf8_prim,
			NULL,
			fields,
			_("Update"), G_CALLBACK(info_modify_ok_cb),
			_("Cancel"), G_CALLBACK(info_modify_cancel_cb),
			purple_connection_get_account(gc), NULL, NULL,
			info_request);

	g_free(utf8_title);
	g_free(utf8_prim);
}

/* process the reply of modify_info packet */
void qq_process_change_info(PurpleConnection *gc, guint8 *data, gint data_len)
{
	qq_data *qd;
	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	data[data_len] = '\0';
	if (qd->uid != atoi((gchar *) data)) {	/* return should be my uid */
		purple_debug_info("QQ", "Failed Updating info\n");
		qq_got_message(gc, _("Could not change buddy information."));
	}
}

static void request_set_buddy_icon(PurpleConnection *gc, gint face_num)
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
	qq_request_buddy_info(gc, qd->uid, 0, QQ_BUDDY_INFO_SET_ICON);
}

void qq_change_icon_cb(PurpleConnection *gc, const char *filepath)
{
	gchar *basename;
	size_t index;
	gint face;

	g_return_if_fail(filepath != NULL);

	purple_debug_info("QQ", "Change my icon to %s\n", filepath);

	basename = g_path_get_basename(filepath);
	index = strcspn(basename, "0123456789");
	face = strtol(basename + index, NULL, 10);
	g_free(basename);
	purple_debug_info("QQ", "Set face to %d\n", face);

	request_set_buddy_icon(gc, face);
}

void qq_set_custom_icon(PurpleConnection *gc, PurpleStoredImage *img)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	const gchar *icon_path = purple_account_get_buddy_icon_path(account);

	g_return_if_fail(icon_path != NULL);

	/* Fixme:
	 *  icon_path is always null
	 *  purple_imgstore_get_filename is always new file
	 *  QQ buddy may set custom icon if level is over 16 */
	purple_debug_info("QQ", "Change my icon to %s\n", icon_path);
}

gchar *qq_get_icon_name(gint face)
{
	gint icon;
	gchar *icon_name;

	icon = face / 3 + 1;
	if (icon < 1 || icon > QQ_FACES) {
		icon = 1;
	}

	icon_name = g_strdup_printf("%s%d%s", QQ_ICON_PREFIX, icon, QQ_ICON_SUFFIX);
	return icon_name;
}

/*
 * This function seems to let people set their buddy icon, but it restricts
 * them to using a small list of stock icons.  Wouldn't it make more sense
 * to use libpurple's normal icon setting stuff?
 *
 * Also it would be nice to unify the icon_dir code for Windows and Linux.
 */
gchar *qq_get_icon_path(gchar *icon_name)
{
	gchar *icon_path;
	const gchar *icon_dir;
#ifdef _WIN32
	static char *dir = NULL;
	if (dir == NULL) {
		dir = g_build_filename(wpurple_install_dir(), "pixmaps",
				"purple", "buddy_icons", "qq", NULL);
	}
#endif

	/*
	 * TODO: The QQ protocol plugin should probably call
	 *       purple_prefs_add_string() at startup to initialize this
	 *       preference.  It is used to allow users or distributions
	 *       to specify this directory.  We don't include these icons
	 *       with libpurple because of possible copyright concerns.
	 */
	icon_dir = purple_prefs_get_string("/plugins/prpl/qq/icon_dir");
	if ( icon_dir == NULL || strlen(icon_dir) == 0) {
#ifdef _WIN32
			icon_dir = dir;
#else
			icon_dir = QQ_BUDDY_ICON_DIR;
#endif
	}
	icon_path = g_strdup_printf("%s%c%s", icon_dir, G_DIR_SEPARATOR, icon_name);

	return icon_path;
}

void qq_update_buddy_icon(PurpleAccount *account, const gchar *who, gint face)
{
	PurpleBuddy *buddy;
	const gchar *icon_name_prev = NULL;
	gchar *icon_name;
	gchar *icon_path;
	gchar *icon_file_content;
	gsize icon_file_size;

	g_return_if_fail(account != NULL && who != NULL);

	/* purple_debug_info("QQ", "Update %s icon to %d\n", who, face); */

	icon_name = qq_get_icon_name(face);
	g_return_if_fail(icon_name != NULL);
	/* purple_debug_info("QQ", "icon file name is %s\n", icon_name); */

	if ((buddy = purple_find_buddy(account, who))) {
		icon_name_prev = purple_buddy_icons_get_checksum_for_user(buddy);
		/*
		purple_debug_info("QQ", "Previous icon is %s\n",
				icon_name_prev != NULL ? icon_name_prev : "(NULL)");
		*/
	}
	if (icon_name_prev != NULL && !strcmp(icon_name, icon_name_prev)) {
		/* purple_debug_info("QQ", "Icon is not changed\n"); */
		g_free(icon_name);
		return;
	}

	icon_path = qq_get_icon_path(icon_name);
	if (icon_path == NULL) {
		g_free(icon_name);
		return;
	}

	if (!g_file_get_contents(icon_path, &icon_file_content, &icon_file_size, NULL)) {
		purple_debug_error("QQ", "Failed reading icon file %s\n", icon_path);
	} else {
		purple_debug_info("QQ", "Update %s icon to %d (%s)\n",
				who, face, icon_path);
		purple_buddy_icons_set_for_user(account, who,
				icon_file_content, icon_file_size, icon_name);
	}
	g_free(icon_name);
	g_free(icon_path);
}

/* after getting info or modify myself, refresh the buddy list accordingly */
static void update_buddy_info(PurpleConnection *gc, gchar **segments)
{
	PurpleBuddy *buddy = NULL;
	qq_data *qd = NULL;
	qq_buddy_data *bd = NULL;
	guint32 uid;
	gchar *who;
	gchar *alias_utf8;

	PurpleAccount *account = purple_connection_get_account(gc);
	qd = (qq_data *)purple_connection_get_protocol_data(gc);

	uid = strtoul(segments[QQ_INFO_UID], NULL, 10);
	who = uid_to_purple_name(uid);
	qq_filter_str(segments[QQ_INFO_NICK]);
	alias_utf8 = qq_to_utf8(segments[QQ_INFO_NICK], QQ_CHARSET_DEFAULT);

	if (uid == qd->uid) {	/* it is me */
		purple_debug_info("QQ", "Got my info\n");
		qd->my_icon = strtol(segments[QQ_INFO_FACE], NULL, 10);
		if (alias_utf8 != NULL) {
			purple_account_set_alias(account, alias_utf8);
		}
		/* add me to buddy list */
		buddy = qq_buddy_find_or_new(gc, uid);
	} else {
		buddy = purple_find_buddy(gc->account, who);
		/* purple_debug_info("QQ", "buddy=%p\n", (void*)buddy); */
	}

	/* if the buddy is null, the api will catch it and return null here */
	bd = purple_buddy_get_protocol_data(buddy);
	/* purple_debug_info("QQ", "bd=%p\n", (void*)bd); */

	if (bd == NULL || buddy == NULL) {
		g_free(who);
		g_free(alias_utf8);
		return;
	}

	/* update buddy list (including myself, if myself is the buddy) */
	bd->age = strtol(segments[QQ_INFO_AGE], NULL, 10);
	bd->gender = strtol(segments[QQ_INFO_GENDER], NULL, 10);
	bd->face = strtol(segments[QQ_INFO_FACE], NULL, 10);

	if (alias_utf8 != NULL) {
		if (bd->nickname) g_free(bd->nickname);
		bd->nickname = g_strdup(alias_utf8);
	}
	bd->last_update = time(NULL);

	purple_blist_server_alias_buddy(buddy, bd->nickname);

	/* convert face num from packet (0-299) to local face (1-100) */
	qq_update_buddy_icon(gc->account, who, bd->face);

	g_free(who);
	g_free(alias_utf8);
}

/* process reply to get_info packet */
void qq_process_get_buddy_info(guint8 *data, gint data_len, guint32 action, PurpleConnection *gc)
{
	qq_data *qd;
	gchar **segments;
	gint field_count;
	gchar *icon_name;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	if (qd->client_version >= 2008) {
		field_count = QQ_INFO_LAST;
	} else {
		field_count = QQ_INFO_LAST_2007;
	}
	if (NULL == (segments = split_data(data, data_len, "\x1e", field_count)))
		return;

#ifdef DEBUG
	info_debug(segments);
#endif

	if (action == QQ_BUDDY_INFO_SET_ICON) {
		if (strtol(segments[QQ_INFO_FACE], NULL, 10) != qd->my_icon) {
			icon_name = g_strdup_printf("%d", qd->my_icon);
			g_free(segments[QQ_INFO_FACE]);
			segments[QQ_INFO_FACE] = icon_name;

			/* Update me in buddy list */
			update_buddy_info(gc, segments);
			/* send new face to server */
			request_change_info(gc, segments);
		}
		g_strfreev(segments);
		return;
	}

	update_buddy_info(gc, segments);
	switch (action) {
		case QQ_BUDDY_INFO_DISPLAY:
			info_display_only(gc, segments);
			break;
		case QQ_BUDDY_INFO_SET_ICON:
			g_return_if_reached();
			break;
		case QQ_BUDDY_INFO_MODIFY_BASE:
			info_modify_dialogue(gc, segments, QQ_FIELD_BASE);
			break;
		case QQ_BUDDY_INFO_MODIFY_EXT:
			info_modify_dialogue(gc, segments, QQ_FIELD_EXT);
			break;
		case QQ_BUDDY_INFO_MODIFY_ADDR:
			info_modify_dialogue(gc, segments, QQ_FIELD_ADDR);
			break;
		case QQ_BUDDY_INFO_MODIFY_CONTACT:
			info_modify_dialogue(gc, segments, QQ_FIELD_CONTACT);
			break;
		default:
			g_strfreev(segments);
			break;
	}
	return;
}

void qq_request_get_level(PurpleConnection *gc, guint32 uid)
{
	qq_data *qd = (qq_data *) gc->proto_data;
	guint8 buf[16] = {0};
	gint bytes = 0;

	if (qd->client_version >= 2007) {
		bytes += qq_put8(buf + bytes, 0x02);
	} else {
		bytes += qq_put8(buf + bytes, 0x00);
	}
	bytes += qq_put32(buf + bytes, uid);
	qq_send_cmd(gc, QQ_CMD_GET_LEVEL, buf, bytes);
}

void qq_request_get_level_2007(PurpleConnection *gc, guint32 uid)
{
	guint8 buf[16] = {0};
	gint bytes = 0;

	bytes += qq_put8(buf + bytes, 0x08);
	bytes += qq_put32(buf + bytes, uid);
	bytes += qq_put8(buf + bytes, 0x00);
	qq_send_cmd(gc, QQ_CMD_GET_LEVEL, buf, bytes);
}

void qq_request_get_buddies_level(PurpleConnection *gc, guint32 update_class)
{
	qq_data *qd = (qq_data *) gc->proto_data;
	PurpleBuddy *buddy;
	qq_buddy_data *bd;
	guint8 *buf;
	GSList *buddies, *it;
	gint bytes;

	/* server only reply levels for online buddies */
	buf = g_newa(guint8, MAX_PACKET_SIZE);

	bytes = 0;
	bytes += qq_put8(buf + bytes, 0x00);
	buddies = purple_find_buddies(purple_connection_get_account(gc), NULL);
	for (it = buddies; it; it = it->next) {
		buddy = it->data;
		if (buddy == NULL) continue;
		if ((bd = purple_buddy_get_protocol_data(buddy)) == NULL) continue;
		if (bd->uid == 0) continue;	/* keep me as end of packet*/
		if (bd->uid == qd->uid) continue;
		bytes += qq_put32(buf + bytes, bd->uid);
	}
	bytes += qq_put32(buf + bytes, qd->uid);
	qq_send_cmd_mess(gc, QQ_CMD_GET_LEVEL, buf, bytes, update_class, 0);
}

static void process_level(PurpleConnection *gc, guint8 *data, gint data_len)
{
	gint bytes = 0;
	guint32 uid, onlineTime;
	guint16 level, timeRemainder;
	qq_buddy_data *bd;

	while (data_len - bytes >= 12) {
		bytes += qq_get32(&uid, data + bytes);
		bytes += qq_get32(&onlineTime, data + bytes);
		bytes += qq_get16(&level, data + bytes);
		bytes += qq_get16(&timeRemainder, data + bytes);
		purple_debug_info("QQ", "level: %d, uid %u, tmOnline: %d, tmRemainder: %d\n",
				level, uid, onlineTime, timeRemainder);

		bd = qq_buddy_data_find(gc, uid);
		if (bd == NULL) {
			purple_debug_error("QQ", "Got levels of %u not in my buddy list\n", uid);
			continue;
		}

		bd->onlineTime = onlineTime;
		bd->level = level;
		bd->timeRemainder = timeRemainder;
	}

	if (bytes != data_len) {
		purple_debug_error("QQ",
				"Wrong format of Get levels. Truncate %d bytes.\n", data_len - bytes);
	}
}

static void process_level_2007(PurpleConnection *gc, guint8 *data, gint data_len)
{
	gint bytes;
	guint32 uid, onlineTime;
	guint16 level, timeRemainder;
	qq_buddy_data *bd;
	guint16 str_len;
	gchar *str;
	gchar *str_utf8;

	bytes = 0;
	bytes += qq_get32(&uid, data + bytes);
	bytes += qq_get32(&onlineTime, data + bytes);
	bytes += qq_get16(&level, data + bytes);
	bytes += qq_get16(&timeRemainder, data + bytes);
	purple_debug_info("QQ", "level: %d, uid %u, tmOnline: %d, tmRemainder: %d\n",
			level, uid, onlineTime, timeRemainder);

	bd = qq_buddy_data_find(gc, uid);
	if (bd == NULL) {
		purple_debug_error("QQ", "Got levels of %u not in my buddy list\n", uid);
		return;
	}

	bd->onlineTime = onlineTime;
	bd->level = level;
	bd->timeRemainder = timeRemainder;

	/* extend bytes in qq2007*/
	bytes += 4;	/* skip 8 bytes */
	/* qq_show_packet("Buddies level", data + bytes, data_len - bytes); */

	do {
		bytes += qq_get16(&str_len, data + bytes);
		if (str_len <= 0 || bytes + str_len > data_len) {
			purple_debug_error("QQ",
					"Wrong format of Get levels. Truncate %d bytes.\n", data_len - bytes);
			break;
		}
		str = g_strndup((gchar *)data + bytes, str_len);
		bytes += str_len;
		str_utf8 = qq_to_utf8(str, QQ_CHARSET_DEFAULT);
		purple_debug_info("QQ", "%s\n", str_utf8);
		g_free(str_utf8);
		g_free(str);
	} while (bytes < data_len);
}

void qq_process_get_level_reply(guint8 *data, gint data_len, PurpleConnection *gc)
{
	gint bytes;
	guint8 sub_cmd;

	bytes = 0;
	bytes += qq_get8(&sub_cmd, data + bytes);
	switch (sub_cmd) {
		case 0x08:
			process_level_2007(gc, data + bytes, data_len - bytes);
			break;
		case 0x00:
		case 0x02:
		default:
			process_level(gc, data + bytes, data_len - bytes);
			break;
	}
}

