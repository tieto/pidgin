/**
 * @file qq.c The QQ2003C protocol plugin
 *
 * gaim
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

#ifdef _WIN32
#define random rand
#endif

#include "accountopt.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "roomlist.h"
#include "server.h"
#include "util.h"

#include "buddy_info.h"
#include "buddy_opt.h"
#include "buddy_status.h"
#include "char_conv.h"
#include "crypt.h"
#include "group.h"
#include "group_find.h"
#include "group_im.h"
#include "group_info.h"
#include "group_join.h"
#include "group_opt.h"
#include "header_info.h"
#include "im.h"
#include "keep_alive.h"
#include "login_logout.h"
#include "packet_parse.h"
#include "qq.h"
#include "qq_proxy.h"
#include "send_core.h"
#include "send_file.h"
#include "utils.h"
#include "version.h"

#define OPENQ_AUTHOR            "Puzzlebird"
#define OPENQ_WEBSITE            "http://openq.sourceforge.net"
#define QQ_TCP_QUERY_PORT       "8000"
#define QQ_UDP_PORT             "8000"

const gchar *udp_server_list[] = {
	"sz.tencent.com",
	"sz2.tencent.com",
	"sz3.tencent.com",
	"sz4.tencent.com",
	"sz5.tencent.com",
	"sz6.tencent.com",
	"sz7.tencent.com",
	"sz8.tencent.com",
	"sz9.tencent.com"
};
const gint udp_server_amount = (sizeof(udp_server_list) / sizeof(udp_server_list[0]));


const gchar *tcp_server_list[] = {
	"tcpconn.tencent.com",
	"tcpconn2.tencent.com",
	"tcpconn3.tencent.com",
	"tcpconn4.tencent.com",
	"tcpconn5.tencent.com",
	"tcpconn6.tencent.com"
};
const gint tcp_server_amount = (sizeof(tcp_server_list) / sizeof(tcp_server_list[0]));

static void _qq_login(GaimAccount *account)
{
	const gchar *qq_server, *qq_port;
	qq_data *qd;
	GaimConnection *gc;
	GaimPresence *presence;	
	gboolean use_tcp;

	g_return_if_fail(account != NULL);

	gc = gaim_account_get_connection(account);
	g_return_if_fail(gc != NULL);

	gc->flags |= GAIM_CONNECTION_HTML | GAIM_CONNECTION_NO_BGCOLOR | GAIM_CONNECTION_AUTO_RESP;

	qd = g_new0(qq_data, 1);
	qd->gc = gc;
	gc->proto_data = qd;

	qq_server = gaim_account_get_string(account, "server", NULL);
	qq_port = gaim_account_get_string(account, "port", NULL);
	use_tcp = gaim_account_get_bool(account, "use_tcp", FALSE);
        presence = gaim_account_get_presence(account);

	qd->use_tcp = use_tcp;

	if(gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_INVISIBLE)) {
		qd->login_mode = QQ_LOGIN_MODE_HIDDEN;
	} else if(gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_AWAY)
				|| gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_EXTENDED_AWAY)) {
		qd->login_mode = QQ_LOGIN_MODE_AWAY;
	} else {
		qd->login_mode = QQ_LOGIN_MODE_NORMAL;
	}

	if (qq_server == NULL || strlen(qq_server) == 0)
		qq_server = use_tcp ?
		    tcp_server_list[random() % tcp_server_amount] : 
		    udp_server_list[random() % udp_server_amount];

	if (qq_port == NULL || strtol(qq_port, NULL, 10) == 0)
		qq_port = use_tcp ? QQ_TCP_QUERY_PORT : QQ_UDP_PORT;

	gaim_connection_update_progress(gc, _("Connecting"), 0, QQ_CONNECT_STEPS);
 
	if (qq_connect(account, qq_server, strtol(qq_port, NULL, 10), use_tcp, FALSE) < 0)
		gaim_connection_error(gc, _("Unable to connect."));
}

/* directly goes for qq_disconnect */
static void _qq_close(GaimConnection *gc)
{
	g_return_if_fail(gc != NULL);
	qq_disconnect(gc);
}

/* returns the icon name for a buddy or protocol */
static const gchar *_qq_list_icon(GaimAccount *a, GaimBuddy *b)
{
	gchar *filename;
	qq_buddy *q_bud;

	/* do not use g_return_val_if_fail, as it is not assertion */
	if (b == NULL || b->proto_data == NULL)
		return "qq";
	
	q_bud = (qq_buddy *) b->proto_data;
	filename = get_icon_name(q_bud->icon / 3 + 1);

	return filename;
}


/* a short status text beside buddy icon*/
static gchar *_qq_status_text(GaimBuddy *b)
{
	qq_buddy *q_bud;
	GString *status;

	q_bud = (qq_buddy *) b->proto_data;
	if (q_bud == NULL)
		return NULL;

	status = g_string_new("");

	switch(q_bud->status) {
	case QQ_BUDDY_OFFLINE:
		g_string_append(status, "Offline");
		break;
	case QQ_BUDDY_ONLINE_NORMAL:
		return NULL;
		break;
	/* TODO What does this status mean? Labelling it as offline... */
	case QQ_BUDDY_ONLINE_OFFLINE:
		g_string_append(status, "Offline");
		break;
	case QQ_BUDDY_ONLINE_AWAY:
		g_string_append(status, "Away");
		break;
	case QQ_BUDDY_ONLINE_INVISIBLE:
		g_string_append(status, "Invisible");
		break;
	default:
		g_string_printf(status, "Unknown-%d", q_bud->status);
	}	

	return g_string_free(status, FALSE);
}


/* a floating text when mouse is on the icon, show connection status here */
static void _qq_tooltip_text(GaimBuddy *b, GString *tooltip, gboolean full)
{
	qq_buddy *q_bud;
	gchar *ip_str;

	g_return_if_fail(b != NULL);

	q_bud = (qq_buddy *) b->proto_data;
	g_return_if_fail(q_bud != NULL);

	if (GAIM_BUDDY_IS_ONLINE(b) && q_bud != NULL)
	{
		ip_str = gen_ip_str(q_bud->ip);
		if (strlen(ip_str) != 0) {
			g_string_append_printf(tooltip, "\n<b>%s Address:</b> %s:%d", 
					(q_bud->comm_flag & QQ_COMM_FLAG_TCP_MODE)
				       ? "TCP" : "UDP", ip_str, q_bud->port);
		}
		g_free(ip_str);
		g_string_append_printf(tooltip, "\n<b>Age:</b> %d", q_bud->age);
        	switch (q_bud->gender) {
	        case QQ_BUDDY_GENDER_GG:
                	g_string_append(tooltip, "\n<b>Gender:</b> Male");
        	        break;
	        case QQ_BUDDY_GENDER_MM:
                	g_string_append(tooltip, "\n<b>Gender:</b> Female");
        	        break;
	        case QQ_BUDDY_GENDER_UNKNOWN:
                	g_string_append(tooltip, "\n<b>Gender:</b> Unknown");
        	        break;
	        default:
                	g_string_append_printf(tooltip, "\n<b>Gender:</b> ERROR(%d)", q_bud->gender);
	        }
		/* For debugging */
		/*
		g_string_append_printf(tooltip, "\n<b>Flag:</b> %01x", q_bud->flag1);
		g_string_append_printf(tooltip, "\n<b>CommFlag:</b> %01x", q_bud->comm_flag);
	        g_string_append_printf(tooltip, "\n<b>Client:</b> %04x", q_bud->client_version);
		*/
	}
}

/* we can show tiny icons on the four corners of buddy icon, */
static void _qq_list_emblems(GaimBuddy *b, const char **se, const char **sw, const char **nw, const char **ne)
{
	/* each char ** are refering to filename in pixmaps/gaim/status/default/ *png */

	qq_buddy *q_bud = b->proto_data;
        const char *emblems[4] = { NULL, NULL, NULL, NULL };
        int i = 1;

        if (q_bud == NULL) {
                emblems[0] = "offline";
	} else {
		/* TODO the wireless icon is a bit too big to look good with QQ faces */
		if (q_bud->status == QQ_BUDDY_ONLINE_AWAY)
			emblems[i++] = "away";
		if (q_bud->comm_flag & QQ_COMM_FLAG_QQ_MEMBER)
			emblems[i++] = "qq_member";
                if (q_bud->comm_flag & QQ_COMM_FLAG_BIND_MOBILE)
                        emblems[i++] = "wireless";
		if (q_bud->comm_flag & QQ_COMM_FLAG_VIDEO)
			emblems[i%4] = "video";

        }

        *se = emblems[0];
        *sw = emblems[1];
        *nw = emblems[2];
        *ne = emblems[3];

	return;
}

/* QQ away status (used to initiate QQ away packet) */
static GList *_qq_away_states(GaimAccount *ga) 
{
	GaimStatusType *status;
	GList *types = NULL;

	status = gaim_status_type_new_full(GAIM_STATUS_AVAILABLE,
			"available", _("QQ: Available"), FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

        status = gaim_status_type_new_full(GAIM_STATUS_AWAY, 
			"away", _("QQ: Away"), FALSE, TRUE, FALSE);
        types = g_list_append(types, status);

        status = gaim_status_type_new_full(GAIM_STATUS_INVISIBLE, 
			"invisible", _("QQ: Invisible"), FALSE, TRUE, FALSE);
        types = g_list_append(types, status);

	status = gaim_status_type_new_full(GAIM_STATUS_OFFLINE,
                        "offline", _("QQ: Offline"), FALSE, TRUE, FALSE);
        types = g_list_append(types, status);

	return types;
}

/* initiate QQ away with proper change_status packet */
static void _qq_set_away(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc = gaim_account_get_connection(account);

	qq_send_packet_change_status(gc);
}

/* IMPORTANT: GaimConvImFlags -> GaimMessageFlags */
/* send an instant msg to a buddy */
static gint _qq_send_im(GaimConnection *gc, const gchar *who, const gchar *message, GaimMessageFlags flags)
{
	gint type, to_uid;
	gchar *msg, *msg_with_qq_smiley;
	qq_data *qd;

	g_return_val_if_fail(who != NULL, -1);

	qd = (qq_data *) gc->proto_data;

	g_return_val_if_fail(strlen(message) <= QQ_MSG_IM_MAX, -E2BIG);

	type = (flags == GAIM_MESSAGE_AUTO_RESP ? QQ_IM_AUTO_REPLY : QQ_IM_TEXT);
	to_uid = gaim_name_to_uid(who);

	/* if msg is to myself, bypass the network */
	if (to_uid == qd->uid) {
		serv_got_im(gc, who, message, flags, time(NULL));
	} else {
		msg = utf8_to_qq(message, QQ_CHARSET_DEFAULT);
		msg_with_qq_smiley = gaim_smiley_to_qq(msg);
		qq_send_packet_im(gc, to_uid, msg_with_qq_smiley, type);
		g_free(msg);
		g_free(msg_with_qq_smiley);
	}

	return 1;
}

/* send a chat msg to a QQ Qun */
static int _qq_chat_send(GaimConnection *gc, int channel, const char *message, GaimMessageFlags flags)
{
	gchar *msg, *msg_with_qq_smiley;
	qq_group *group;

	g_return_val_if_fail(message != NULL, -1);
	g_return_val_if_fail(strlen(message) <= QQ_MSG_IM_MAX, -E2BIG);

	group = qq_group_find_by_channel(gc, channel);
	g_return_val_if_fail(group != NULL, -1);

	msg = utf8_to_qq(message, QQ_CHARSET_DEFAULT);
	msg_with_qq_smiley = gaim_smiley_to_qq(msg);
	qq_send_packet_group_im(gc, group, msg_with_qq_smiley);
	g_free(msg);
	g_free(msg_with_qq_smiley);

	return 1;
}

/* send packet to get who's detailed information */
static void _qq_get_info(GaimConnection *gc, const gchar *who)
{
	guint32 uid;
	qq_data *qd;

	qd = gc->proto_data;
	uid = gaim_name_to_uid(who);

	if (uid <= 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Not valid QQid: %s\n", who);
		gaim_notify_error(gc, NULL, _("Invalid name"), NULL);
		return;
	}

	qq_send_packet_get_info(gc, uid, TRUE);
}

/* get my own information */
static void _qq_menu_modify_my_info(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	qq_data *qd;

	qd = (qq_data *) gc->proto_data;
	qq_prepare_modify_info(gc);
}

static void _qq_change_face_cb(GaimConnection *gc, GaimRequestFields *fields)
{
	qq_data *qd;
	GaimRequestField *field;
	gint suffix;
	
	qd = (qq_data *) gc->proto_data;

	field = gaim_request_fields_get_field(fields, "face_num");
	suffix = get_icon_offset(gc);
	qd->my_icon = gaim_request_field_choice_get_value(field) * 3 + suffix;
	qd->modifying_face = TRUE;
	qq_send_packet_get_info(gc, qd->uid, FALSE);
}

static void _qq_add_face_choice(GaimRequestFieldGroup *group, gint face_num)
{
	GaimRequestField *field;
	struct stat img_stat;
	FILE *file;
	gchar *filename, *img_data;
	gchar face[15];
	gint size;

	g_snprintf(face, sizeof(face), "qq_%i.png", face_num);
	filename = g_build_filename(DATADIR, "pixmaps",
			"gaim","status","default", face, NULL);
	g_snprintf(face, sizeof(face), "%i", face_num);
	file = g_fopen(filename, "rb");
	if (file && fstat(fileno(file), &img_stat) == 0) {
		img_data = g_malloc(img_stat.st_size);
		size = fread(img_data, 1, img_stat.st_size, file);

		field = gaim_request_field_image_new(face, face, img_data, size);
		gaim_request_field_group_add_field(group, field);

		g_free(img_data);
		fclose(file);
	}
}

/* Change your status icon (face) */
static void _qq_menu_change_face(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	qq_data *qd = (qq_data *) gc->proto_data;
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;
	gchar label[15];
	gint i;

	fields = gaim_request_fields_new();
	group = gaim_request_field_group_new(_("Selection"));
	gaim_request_fields_add_group(fields, group);
	field = gaim_request_field_choice_new("face_num",
		       	_("Select a number"), qd->my_icon / 3);
	for(i = 1; i <= QQ_FACES; i++) {
		g_snprintf(label, sizeof(label), "%i", i);
		gaim_request_field_choice_add(field, label);
	}
	gaim_request_field_group_add_field(group, field);
	group = gaim_request_field_group_new(_("Faces"));
	gaim_request_fields_add_group(fields, group);
	for(i = 1; i <= QQ_FACES; i++)
		_qq_add_face_choice(group, i);

	gaim_request_fields(gc, _("Change Your QQ Face"),
			_("Change Face"), NULL, fields,
			_("Update"), G_CALLBACK(_qq_change_face_cb),
			_("Cancel"), NULL,
			gc);
}

static void _qq_menu_change_password(GaimPluginAction *action)
{
	gaim_notify_uri(NULL, "https://password.qq.com");
}

/* remove a buddy from my list and remove myself from his list */
/* TODO: re-enable this
static void _qq_menu_block_buddy(GaimBlistNode * node)
{
	guint32 uid;
	gc_and_uid *g;
	GaimBuddy *buddy;
	GaimConnection *gc;
//	const gchar *who = param_who;	gfhuang
	const gchar *who;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);
	who = buddy->name;
	g_return_if_fail(who != NULL);

	uid = gaim_name_to_uid(who);
	g_return_if_fail(uid > 0);

	g = g_new0(gc_and_uid, 1);
	g->gc = gc;
	g->uid = uid;

	gaim_request_action(gc, _("Block Buddy"),
			    _("Are you sure to block this buddy?"), NULL,
			    1, g, 2,
			    _("Cancel"),
			    G_CALLBACK(qq_do_nothing_with_gc_and_uid),
			    _("Block"), G_CALLBACK(qq_block_buddy_with_gc_and_uid));
}
*/

/* show a brief summary of what we get from login packet */
static void _qq_menu_show_login_info(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	qq_data *qd;
	GString *info;

	qd = (qq_data *) gc->proto_data;
	info = g_string_new("<html><body>\n");

	g_string_append_printf(info, _("<b>Current Online</b>: %d<br>\n"), qd->all_online);
	g_string_append_printf(info, _("<b>Last Refresh</b>: %s<br>\n"), ctime(&qd->last_get_online));

	g_string_append(info, "<hr>\n");

	g_string_append_printf(info, _("<b>Connection Mode</b>: %s<br>\n"), qd->use_tcp ? "TCP" : "UDP");
	g_string_append_printf(info, _("<b>Server IP</b>: %s: %d<br>\n"), qd->server_ip, qd->server_port);
	g_string_append_printf(info, _("<b>My Public IP</b>: %s<br>\n"), qd->my_ip);

	g_string_append(info, "<hr>\n");
	g_string_append(info, "<i>Information below may not be accurate</i><br>\n");

	g_string_append_printf(info, _("<b>Login Time</b>: %s<br>\n"), ctime(&qd->login_time));
	g_string_append_printf(info, _("<b>Last Login IP</b>: %s<br>\n"), qd->last_login_ip);
	g_string_append_printf(info, _("<b>Last Login Time</b>: %s\n"), ctime(&qd->last_login_time));

	g_string_append(info, "</body></html>");

	gaim_notify_formatted(gc, NULL, _("Login Information"), NULL, info->str, NULL, NULL);

	g_string_free(info, TRUE);
}

/*
static void _qq_menu_search_or_add_permanent_group(GaimPluginAction *action)
{
	gaim_roomlist_show_with_account(NULL);
}
*/

/*
static void _qq_menu_create_permanent_group(GaimPluginAction * action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_request_input(gc, _("Create QQ Qun"),
			   _("Input Qun name here"),
			   _("Only QQ member can create permanent Qun"),
			   "OpenQ", FALSE, FALSE, NULL,
			   _("Create"), G_CALLBACK(qq_group_create_with_name), _("Cancel"), NULL, gc);
}
*/

static void _qq_menu_unsubscribe_group(GaimBlistNode * node)
{
	GaimChat *chat = (GaimChat *)node;
	GaimConnection *gc = gaim_account_get_connection(chat->account);
	GHashTable *components = chat -> components;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	g_return_if_fail(components != NULL);
	qq_group_exit(gc, components);
}

/*
static void _qq_menu_manage_group(GaimBlistNode * node)
{
	GaimChat *chat = (GaimChat *)node;
	GaimConnection *gc = gaim_account_get_connection(chat->account);
	GHashTable *components = chat -> components;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	g_return_if_fail(components != NULL);
	qq_group_manage_group(gc, components);
}
*/

/* TODO: re-enable this
static void _qq_menu_send_file(GaimBlistNode * node, gpointer ignored)
{
	GaimBuddy *buddy;
	GaimConnection *gc;
	qq_buddy *q_bud;

	g_return_if_fail (GAIM_BLIST_NODE_IS_BUDDY (node));
	buddy = (GaimBuddy *) node;
	q_bud = (qq_buddy *) buddy->proto_data;
//	if (is_online (q_bud->status)) {
	gc = gaim_account_get_connection (buddy->account);
	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qq_send_file(gc, buddy->name, NULL);
//	}
}
*/

/* attempt to output the value and byte length of a given field */
/*
static gboolean _qq_parse_custom_packet_field(GaimRequestFields *fields,
		const gchar *id, guint8 **value, gint *len, gboolean allow_null)
{
	GaimRequestField *field;
	const gchar *str;
	gint i;
	gboolean success;

	success = FALSE;
	field = gaim_request_fields_get_field(fields, id);
	str = gaim_request_field_string_get_value(field);
	if (!str && allow_null) {
		return TRUE;
	} else if (str) {
		success = TRUE;
		if (strcmp(id, "uid") != 0) {
			*value = hex_str_to_bytes(str, len);
			if (!*value)
				success = FALSE;
		} else {
			for (i = 0; i < strlen(str); i++) {
				if (!g_ascii_isdigit(str[i])) {
					success = FALSE;
					break;
				}
			}
			if (success) {
				*(guint32 *) value = strtoul(str, NULL, 10);
				if (errno == ERANGE)
					success = FALSE;
			}
		}
	}
	if (!success)
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Invalid entry: %s\n", id);
	return success;
}
*/

/* attempt to output the field values and body length */
/*
static gboolean _qq_parse_custom_packet_fields(GaimRequestFields *fields,
		guint8 **client, guint8 **cmd, guint8 **seq, guint32 *uid, 
		guint8 **body, guint8 **key, gint *body_len)
{
	gint len;
	gboolean success;

       success = FALSE;
       *client = *cmd = *seq = *body = *key = NULL;
       *uid = *body_len = 0;
       if ((_qq_parse_custom_packet_field(fields, "client", client, &len, FALSE)
               && len == 2
               && _qq_parse_custom_packet_field(fields, "cmd", cmd, &len, FALSE)
               && len == 2
               && _qq_parse_custom_packet_field(fields, "uid", (guint8 **) uid, &len, FALSE)
               && _qq_parse_custom_packet_field(fields, "seq", seq, &len, FALSE)
               && len == 2
               && _qq_parse_custom_packet_field(fields, "body", body, body_len, TRUE))) {
               if (*body_len > MAX_PACKET_SIZE / 8) {
                       g_free(*client);
                       g_free(*cmd);
                       g_free(*seq);
                       g_free(*body);
                       return FALSE;
               }
               if (!gaim_request_fields_get_bool(fields, "encrypt"))
                       return TRUE;
               else
                       success = _qq_parse_custom_packet_field(fields,
                                       "key", key, &len, FALSE)
                               && len == 16
                               && *body_len > 0;
       }
       if (!success) {
		if (*client)
			g_free(*client);
		if (*cmd)
			g_free(*cmd);
		if (*seq)
			g_free(*seq);
		if (*key)
			g_free(*key);
		return FALSE;
	}
	return TRUE;
}
*/

/* parses the request fields and attempts to send the packet */
/*
static void _qq_send_custom_packet_cb(GaimConnection *gc, GaimRequestFields *fields)
{
	guint32 uid;
	guint8 *buf, *client, *cmd, *seq, *body, *encr_body, *key, *cursor;
	gint bytes, len, encr_len;
	qq_data *qd;
	gboolean success;

	qd = (qq_data *) gc->proto_data;

	success = _qq_parse_custom_packet_fields(fields, &client, &cmd, 
			&seq, &uid, &body, &key, &len);
	if (!success) {
		gaim_notify_error(gc, _("Error"), _("Invalid entry"), NULL);
		return;
	}

	bytes = 0;
	buf = g_newa(guint8, MAX_PACKET_SIZE);
	cursor = buf;
*/
        /* QQ TCP packet has two bytes in the beginning to define packet length
	 * so I leave room here for size */
/*
	if (qd->use_tcp)
		bytes += create_packet_w(buf, &cursor, 0x0000);
	bytes += create_packet_b(buf, &cursor, QQ_PACKET_TAG);
	bytes += create_packet_w(buf, &cursor, *(guint16 *) client);
	bytes += create_packet_w(buf, &cursor, *(guint16 *) cmd);
	bytes += create_packet_w(buf, &cursor, *(guint16 *) seq);
	bytes += create_packet_dw(buf, &cursor, uid);
	if (body) {
               if (gaim_request_fields_get_bool(fields, "encrypt")) {
                       if (gaim_request_fields_get_bool(fields, "prepend"))
                               bytes += create_packet_data(buf, &cursor, key, 16);
                       encr_body = g_newa(guint8, MAX_PACKET_SIZE);
                       qq_crypt(ENCRYPT, body, len, key, encr_body, &encr_len);
                       bytes += create_packet_data(buf, &cursor, encr_body, encr_len);
                       g_free(key);
               } else {
                       bytes += create_packet_data(buf, &cursor, body, len);
               }

		g_free(body);
	}
	bytes += create_packet_b(buf, &cursor, QQ_PACKET_TAIL);

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Custom packet of length %i\n", bytes);
	_qq_show_packet("Outgoing custom packet", buf, bytes);
	
	_qq_send_packet(gc, buf, bytes, *(guint16 *) cmd);
	g_free(client);
	g_free(cmd);
	g_free(seq);
}
*/

/* send a custom packet to the server - for protocol testing */
/*
static void _qq_menu_send_custom_packet(GaimPluginAction *action)
{
	GaimConnection *gc;
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;
	gchar tmp[15];
	qq_data *qd;

	gc = (GaimConnection *) action->context;
	qd = (qq_data *) gc->proto_data;

	fields = gaim_request_fields_new();
	group = gaim_request_field_group_new(_("Basic Elements"));
	gaim_request_fields_add_group(fields, group);
	g_snprintf(tmp, sizeof(tmp), "%04X", QQ_CLIENT);
	field = gaim_request_field_string_new("client", _("Client (hex)"), tmp, FALSE);
	gaim_request_field_group_add_field(group, field);
	field = gaim_request_field_string_new("cmd", _("Command (hex)"), "0000", FALSE);
	gaim_request_field_group_add_field(group, field);
	field = gaim_request_field_string_new("seq", _("Sequence (hex)"), "0000", FALSE);
	gaim_request_field_group_add_field(group, field);
	g_snprintf(tmp, sizeof(tmp), "%u", qd->uid);
	field = gaim_request_field_string_new("uid", _("QQ Number (decimal)"), tmp, FALSE);
	gaim_request_field_group_add_field(group, field);
	field = gaim_request_field_string_new("body", _("Body (hex)"), NULL, TRUE);
	gaim_request_field_group_add_field(group, field);
	group = gaim_request_field_group_new(_("Encryption"));
	gaim_request_fields_add_group(fields, group);
	field = gaim_request_field_bool_new("encrypt", _("Encrypt Packet Body"), FALSE);
	gaim_request_field_group_add_field(group, field);
	field = gaim_request_field_bool_new("prepend", _("Prepend Key to Body"), FALSE);
	gaim_request_field_group_add_field(group, field);
	field = gaim_request_field_string_new("key", _("Encryption Key (hex)"), NULL, FALSE);
	gaim_request_field_group_add_field(group, field);


	gaim_request_fields(gc, _("Send a custom packet"),
			_("Send a custom packet"), NULL, fields,
			_("Send"), G_CALLBACK(_qq_send_custom_packet_cb),
			_("Cancel"), NULL,
			gc);
}
*/

/* protocol related menus */
static GList *_qq_actions(GaimPlugin *plugin, gpointer context)
{
	GList *m;
	GaimPluginAction *act;

	m = NULL;
	act = gaim_plugin_action_new(_("Modify My Information"), _qq_menu_modify_my_info);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Change My Face"), _qq_menu_change_face);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Change Password"), _qq_menu_change_password);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Show Login Information"), _qq_menu_show_login_info);
	m = g_list_append(m, act);

/*
	act = gaim_plugin_action_new(_("Send Custom Packet"), _qq_menu_send_custom_packet);
	m = g_list_append(m, act);
*/

	/*
	act = gaim_plugin_action_new(_("Qun: Search a permanent Qun"), _qq_menu_search_or_add_permanent_group);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Qun: Create a permanent Qun"), _qq_menu_create_permanent_group);
	m = g_list_append(m, act);
	*/

	return m;
}

/* chat-related (QQ Qun) menu shown up with right-click */
static GList *_qq_chat_menu(GaimBlistNode *node)
{
	GList *m;
	GaimMenuAction *act;

	m = NULL;
	act = gaim_menu_action_new(_("Exit this QQ Qun"), GAIM_CALLBACK(_qq_menu_unsubscribe_group), NULL, NULL);
	m = g_list_append(m, act);

	/* TODO: enable this
	act = gaim_menu_action_new(_("Show Details"), GAIM_CALLBACK(_qq_menu_manage_group), NULL, NULL);
	m = g_list_append(m, act);
	*/

	return m;
}

/* buddy-related menu shown up with right-click */
static GList *_qq_buddy_menu(GaimBlistNode * node)
{
	GList *m;

	if(GAIM_BLIST_NODE_IS_CHAT(node))
		return _qq_chat_menu(node);
	
	m = NULL;
	return m;
}

/* TODO : not working, temp commented out by gfhuang 

	act = gaim_menu_action_new(_("Block this buddy"), GAIM_CALLBACK(_qq_menu_block_buddy), NULL, NULL); //add NULL by gfhuang
	m = g_list_append(m, act);
//	if (q_bud && is_online(q_bud->status)) {
		act = gaim_menu_action_new(_("Send File"), GAIM_CALLBACK(_qq_menu_send_file), NULL, NULL); //add NULL by gfhuang
		m = g_list_append(m, act);
//	}
*/
/*
	return m;
}
*/


static void _qq_keep_alive(GaimConnection *gc)
{
	qq_group *group;
	qq_data *qd;
	GList *list;

	if (NULL == (qd = (qq_data *) gc->proto_data))
		return;

	list = qd->groups;
	while (list != NULL) {
		group = (qq_group *) list->data;
		if (group->my_status == QQ_GROUP_MEMBER_STATUS_IS_MEMBER ||
		    group->my_status == QQ_GROUP_MEMBER_STATUS_IS_ADMIN)
			/* no need to get info time and time again, online members enough */
			qq_send_cmd_group_get_online_members(gc, group);
	
		list = list->next;
	}

	qq_send_packet_keep_alive(gc);

}

/* convert chat nickname to qq-uid to get this buddy info */
/* who is the nickname of buddy in QQ chat-room (Qun) */
static void _qq_get_chat_buddy_info(GaimConnection *gc, gint channel, const gchar *who)
{
	gchar *gaim_name;
	g_return_if_fail(who != NULL);

	gaim_name = chat_name_to_gaim_name(who);
	if (gaim_name != NULL)
		_qq_get_info(gc, gaim_name);
}

/* convert chat nickname to qq-uid to invite individual IM to buddy */
/* who is the nickname of buddy in QQ chat-room (Qun) */
static gchar *_qq_get_chat_buddy_real_name(GaimConnection *gc, gint channel, const gchar *who)
{
	g_return_val_if_fail(who != NULL, NULL);
	return chat_name_to_gaim_name(who);
}

void qq_function_not_implemented(GaimConnection *gc)
{
	gaim_notify_warning(gc, NULL, 
			_("This function has not be implemented yet"), _("Please wait for new version"));
}

GaimPlugin *my_protocol = NULL;
static GaimPluginProtocolInfo prpl_info	= {
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_USE_POINTSIZE,
	NULL,				/* user_splits	*/
	NULL,				/* protocol_options */
	NO_BUDDY_ICONS,			/* icon_spec */
	_qq_list_icon,			/* list_icon */
	_qq_list_emblems,		/* list_emblems */
	_qq_status_text,		/* status_text	*/
	_qq_tooltip_text,		/* tooltip_text */
	_qq_away_states,		/* away_states	*/
	_qq_buddy_menu,			/* blist_node_menu */
	qq_chat_info,			/* chat_info */
	qq_chat_info_defaults,		/* chat_info_defaults */
	_qq_login,			/* login */
	_qq_close,			/* close */
	_qq_send_im,			/* send_im */
	NULL,				/* set_info */
	NULL,				/* send_typing	*/
	_qq_get_info,			/* get_info */
	_qq_set_away,			/* set_away */
	NULL,				/* set_idle */
	NULL,				/* change_passwd */
	qq_add_buddy,			/* add_buddy */
	NULL,				/* add_buddies	*/
	qq_remove_buddy,		/* remove_buddy */
	NULL,				/* remove_buddies */
	NULL,				/* add_permit */
	NULL,				/* add_deny */
	NULL,				/* rem_permit */
	NULL,				/* rem_deny */
	NULL,				/* set_permit_deny */
	qq_group_join,			/* join_chat */
	NULL,				/* reject chat	invite */
	NULL,				/* get_chat_name */
	NULL,				/* chat_invite	*/
	NULL,				/* chat_leave */
	NULL,				/* chat_whisper */
	_qq_chat_send,			/* chat_send */
	_qq_keep_alive,			/* keepalive */
	NULL,				/* register_user */
	_qq_get_chat_buddy_info,	/* get_cb_info	*/
	NULL,				/* get_cb_away	*/
	NULL,				/* alias_buddy	*/
	NULL,				/* group_buddy	*/
	NULL,				/* rename_group */
	NULL,				/* buddy_free */     
	NULL,				/* convo_closed */
	NULL,				/* normalize */
	NULL,				/* set_buddy_icon */
	NULL,				/* remove_group */
	_qq_get_chat_buddy_real_name,	/* get_cb_real_name */
	NULL,				/* set_chat_topic */
	NULL,				/* find_blist_chat */
	qq_roomlist_get_list,		/* roomlist_get_list */
	qq_roomlist_cancel,		/* roomlist_cancel */
	NULL,				/* roomlist_expand_category */
	NULL,				/* can_receive_file */
	qq_send_file,			/* send_file */
	NULL,                           /* new xfer */
	NULL,				/* offline_message */
	NULL,				/* GaimWhiteboardPrplOps */
	NULL,				/* send_raw */
};

static GaimPluginInfo info = {
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,		/**< type		*/
	NULL,				/**< ui_requirement	*/
	0,				/**< flags		*/
	NULL,				/**< dependencies	*/
	GAIM_PRIORITY_DEFAULT,		/**< priority		*/

	"prpl-qq",			/**< id			*/
	"QQ",				/**< name		*/
	VERSION,			/**< version		*/
					/**  summary		*/
	N_("QQ Protocol	Plugin"),
					/**  description	*/
	N_("QQ Protocol	Plugin"),
	OPENQ_AUTHOR,			/**< author		*/
	OPENQ_WEBSITE,			/**< homepage		*/

	NULL,				/**< load		*/
	NULL,				/**< unload		*/
	NULL,				/**< destroy		*/

	NULL,				/**< ui_info		*/
	&prpl_info,			/**< extra_info		*/
	NULL,				/**< prefs_info		*/
	_qq_actions
};


static void init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_bool_new(_("Login in TCP"), "use_tcp", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_bool_new(_("Login Hidden"), "hidden", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Server"), "server", NULL);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Port"), "port", NULL);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	my_protocol = plugin;

	gaim_prefs_add_none("/plugins/prpl/qq");
	gaim_prefs_add_bool("/plugins/prpl/qq/show_status_by_icon", TRUE);
	gaim_prefs_add_bool("/plugins/prpl/qq/show_fake_video", FALSE);
	gaim_prefs_add_bool("/plugins/prpl/qq/prompt_for_missing_packet", TRUE);
	gaim_prefs_add_bool("/plugins/prpl/qq/prompt_group_msg_on_recv", TRUE);
}

GAIM_INIT_PLUGIN(qq, init_plugin, info);
