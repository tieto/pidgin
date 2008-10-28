/**
 * @file buddy_opt.c
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

#include "debug.h"
#include "internal.h"
#include "notify.h"
#include "request.h"
#include "privacy.h"

#include "buddy_info.h"
#include "buddy_list.h"
#include "buddy_opt.h"
#include "char_conv.h"
#include "qq_define.h"
#include "im.h"
#include "qq_base.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "utils.h"

#define PURPLE_GROUP_QQ_FORMAT          "QQ (%s)"

#define QQ_REMOVE_SELF_REPLY_OK       0x00

enum {
	QQ_MY_AUTH_APPROVE = 0x30,	/* ASCII value of "0" */
	QQ_MY_AUTH_REJECT = 0x31,	/* ASCII value of "1" */
	QQ_MY_AUTH_REQUEST = 0x32,	/* ASCII value of "2" */
};

typedef struct _qq_buddy_req {
	PurpleConnection *gc;
	guint32 uid;
	guint8 *auth;
	guint8 auth_len;
} qq_buddy_req;

void buddy_add_authorize_input(PurpleConnection *gc, guint32 uid,
		guint8 *auth, guint8 auth_len);

static void buddy_add_req_free(qq_buddy_req *add_req)
{
	g_return_if_fail(add_req != NULL);
	if (add_req->auth) g_free(add_req->auth);
	g_free(add_req);
}

PurpleGroup *qq_group_find_or_new(const gchar *group_name)
{
	PurpleGroup *g;

	g_return_val_if_fail(group_name != NULL, NULL);

	g = purple_find_group(group_name);
	if (g == NULL) {
		g = purple_group_new(group_name);
		purple_blist_add_group(g, NULL);
		purple_debug_warning("QQ", "Add new group: %s\n", group_name);
	}

	return g;
}

static qq_buddy_data *qq_buddy_data_new(guint32 uid)
{
	qq_buddy_data *bd = g_new0(qq_buddy_data, 1);
	memset(bd, 0, sizeof(qq_buddy_data));
	bd->uid = uid;
	bd->status = QQ_BUDDY_OFFLINE;
	return bd;
}

qq_buddy_data *qq_buddy_data_find(PurpleConnection *gc, guint32 uid)
{
	gchar *who;
	PurpleBuddy *buddy;

	g_return_val_if_fail(gc != NULL, NULL);

	who = uid_to_purple_name(uid);
	if (who == NULL)	return NULL;
	buddy = purple_find_buddy(purple_connection_get_account(gc), who);
	g_free(who);

	if (buddy == NULL) {
		purple_debug_error("QQ", "Can not find purple buddy of %d\n", uid);
		return NULL;
	}
	if (buddy->proto_data == NULL) {
		purple_debug_error("QQ", "Can not find buddy data of %d\n", uid);
		return NULL;
	}
	return (qq_buddy_data *)buddy->proto_data;
}

void qq_buddy_data_free(qq_buddy_data *bd)
{
	g_return_if_fail(bd != NULL);

	if (bd->nickname) g_free(bd->nickname);
	g_free(bd);
}

/* create purple buddy without data and display with no-auth icon */
PurpleBuddy *qq_buddy_new(PurpleConnection *gc, guint32 uid)
{
	PurpleBuddy *buddy;
	PurpleGroup *group;
	gchar *who;
	gchar *group_name;

	g_return_val_if_fail(gc->account != NULL && uid != 0, NULL);

	group_name = g_strdup_printf(PURPLE_GROUP_QQ_FORMAT,
			purple_account_get_username(gc->account));
	group = qq_group_find_or_new(group_name);
	if (group == NULL) {
		purple_debug_error("QQ", "Failed creating group %s\n", group_name);
		return NULL;
	}

	who = uid_to_purple_name(uid);

	purple_debug_info("QQ", "Add new purple buddy: [%s]\n", who);
	buddy = purple_buddy_new(gc->account, who, NULL);	/* alias is NULL */
	buddy->proto_data = NULL;

	g_free(who);

	purple_blist_add_buddy(buddy, NULL, group, NULL);

	g_free(group_name);

	return buddy;
}

static void qq_buddy_free(PurpleBuddy *buddy)
{
	g_return_if_fail(buddy);
	if (buddy->proto_data)	{
		qq_buddy_data_free(buddy->proto_data);
	}
	buddy->proto_data = NULL;
	purple_blist_remove_buddy(buddy);
}

PurpleBuddy *qq_buddy_find(PurpleConnection *gc, guint32 uid)
{
	PurpleBuddy *buddy;
	gchar *who;

	g_return_val_if_fail(gc->account != NULL && uid != 0, NULL);

	who = uid_to_purple_name(uid);
	buddy = purple_find_buddy(gc->account, who);
	g_free(who);
	return buddy;
}

PurpleBuddy *qq_buddy_find_or_new(PurpleConnection *gc, guint32 uid)
{
	PurpleBuddy *buddy;

	g_return_val_if_fail(gc->account != NULL && uid != 0, NULL);

	buddy = qq_buddy_find(gc, uid);
	if (buddy == NULL) {
		buddy = qq_buddy_new(gc, uid);
		if (buddy == NULL) {
			return NULL;
		}
	}

	if (buddy->proto_data != NULL) {
		return buddy;
	}

	buddy->proto_data = qq_buddy_data_new(uid);
	return buddy;
}

/* send packet to remove a buddy from my buddy list */
static void request_buddy_remove(PurpleConnection *gc, guint32 uid)
{
	gchar uid_str[11];
	gint bytes;

	g_return_if_fail(uid > 0);

	g_snprintf(uid_str, sizeof(uid_str), "%d", uid);
	bytes = strlen(uid_str);
	qq_send_cmd_mess(gc, QQ_CMD_BUDDY_REMOVE, (guint8 *) uid_str, bytes, 0, uid);
}

static void request_buddy_remove_2007(PurpleConnection *gc,
		guint32 uid, guint8 *auth, guint8 auth_len)
{
	gint bytes;
	guint8 *raw_data;
	gchar uid_str[16];

	g_return_if_fail(uid != 0);
	g_return_if_fail(auth != NULL && auth_len > 0);

	raw_data = g_newa(guint8, auth_len + sizeof(uid_str) );
	bytes = 0;
	bytes += qq_put8(raw_data + bytes, auth_len);
	bytes += qq_putdata(raw_data + bytes, auth, auth_len);

	g_snprintf(uid_str, sizeof(uid_str), "%d", uid);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)uid_str, strlen(uid_str));

	qq_send_cmd_mess(gc, QQ_CMD_BUDDY_REMOVE, raw_data, bytes, 0, uid);
}

void qq_request_auth_info(PurpleConnection *gc, guint8 cmd, guint16 sub_cmd, guint32 uid)
{
	guint8 raw_data[16];
	gint bytes;

	g_return_if_fail(uid > 0);
	bytes = 0;
	bytes += qq_put8(raw_data + bytes, cmd);
	bytes += qq_put16(raw_data + bytes, sub_cmd);
	bytes += qq_put32(raw_data + bytes, uid);

	qq_send_cmd_mess(gc, QQ_CMD_AUTH_INFO, raw_data, bytes, 0, uid);
}

void qq_process_auth_info(PurpleConnection *gc, guint8 *data, gint data_len, guint32 uid)
{
	qq_data *qd;
	gint bytes;
	guint8 cmd, reply;
	guint16 sub_cmd;
	guint8 *auth = NULL;
	guint8 auth_len = 0;

	g_return_if_fail(data != NULL && data_len != 0);
	g_return_if_fail(uid != 0);

	qd = (qq_data *) gc->proto_data;

	qq_show_packet("qq_process_auth_info", data, data_len);
	bytes = 0;
	bytes += qq_get8(&cmd, data + bytes);
	bytes += qq_get16(&sub_cmd, data + bytes);
	bytes += qq_get8(&reply, data + bytes);
	if (bytes + 2 <= data_len) {
		bytes += 1;	/* skip 1 byte, 0x00 */
		bytes += qq_get8(&auth_len, data + bytes);
		if (auth_len > 0) {
			g_return_if_fail(bytes + auth_len <= data_len);
			auth = g_newa(guint8, auth_len);
			bytes += qq_getdata(auth, auth_len, data + bytes);
		}
	} else {
		qq_show_packet("No auth info", data, data_len);
	}

	if (cmd == QQ_AUTH_INFO_BUDDY && sub_cmd == QQ_AUTH_INFO_REMOVE_BUDDY) {
		g_return_if_fail(auth != NULL && auth_len > 0);
		request_buddy_remove_2007(gc, uid, auth, auth_len);
		return;
	}
	if (cmd == QQ_AUTH_INFO_BUDDY && sub_cmd == QQ_AUTH_INFO_ADD_BUDDY) {
		buddy_add_authorize_input(gc, uid, auth, auth_len);
		return;
	}
	purple_debug_info("QQ", "Got auth info cmd 0x%x, sub 0x%x, reply 0x%x\n",
			cmd, sub_cmd, reply);
}

/* try to remove myself from someone's buddy list */
static void request_buddy_remove_me(PurpleConnection *gc, guint32 uid)
{
	guint8 raw_data[16] = {0};
	gint bytes = 0;

	g_return_if_fail(uid > 0);

	bytes += qq_put32(raw_data + bytes, uid);

	qq_send_cmd_mess(gc, QQ_CMD_REMOVE_ME, raw_data, bytes, 0, uid);
}

/* try to add a buddy without authentication */
static void request_buddy_add_no_auth(PurpleConnection *gc, guint32 uid)
{
	gchar uid_str[11];

	g_return_if_fail(uid > 0);

	/* we need to send the ascii code of this uid to qq server */
	g_snprintf(uid_str, sizeof(uid_str), "%d", uid);
	qq_send_cmd_mess(gc, QQ_CMD_BUDDY_ADD_NO_AUTH,
			(guint8 *) uid_str, strlen(uid_str), 0, uid);
}

static void request_buddy_add_no_auth_ex(PurpleConnection *gc, guint32 uid)
{
	gchar uid_str[11];

	g_return_if_fail(uid > 0);

	/* we need to send the ascii code of this uid to qq server */
	g_snprintf(uid_str, sizeof(uid_str), "%d", uid);
	qq_send_cmd_mess(gc, QQ_CMD_BUDDY_ADD_NO_AUTH_EX,
			(guint8 *) uid_str, strlen(uid_str), 0, uid);
}

/* this buddy needs authentication, text conversion is done at lowest level */
static void request_buddy_auth(PurpleConnection *gc, guint32 uid, const gchar response, const gchar *text)
{
	gchar *text_qq, uid_str[11];
	guint8 bar, *raw_data;
	gint bytes = 0;

	g_return_if_fail(uid != 0);

	g_snprintf(uid_str, sizeof(uid_str), "%d", uid);
	bar = 0x1f;
	raw_data = g_newa(guint8, QQ_MSG_IM_MAX);

	bytes += qq_putdata(raw_data + bytes, (guint8 *) uid_str, strlen(uid_str));
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_put8(raw_data + bytes, response);

	if (text != NULL) {
		text_qq = utf8_to_qq(text, QQ_CHARSET_DEFAULT);
		bytes += qq_put8(raw_data + bytes, bar);
		bytes += qq_putdata(raw_data + bytes, (guint8 *) text_qq, strlen(text_qq));
		g_free(text_qq);
	}

	qq_send_cmd(gc, QQ_CMD_BUDDY_ADD_AUTH, raw_data, bytes);
}

static void request_buddy_auth_2007(PurpleConnection *gc, guint32 uid,
	const gchar *text, guint8 *auth, guint8 auth_len)
{
	guint8 *raw_data;
	gint bytes = 0;
	gchar *msg;

	g_return_if_fail(uid != 0);

	raw_data = g_newa(guint8, QQ_MSG_IM_MAX);
	bytes = 0;
	bytes += qq_put8(raw_data + bytes, 0x02);
	bytes += qq_put32(raw_data + bytes, uid);
	bytes += qq_put16(raw_data + bytes, 0);

	bytes += qq_put8(raw_data + bytes, 0);
	if (auth == NULL || auth_len <= 0) {
		bytes += qq_put8(raw_data + bytes, 0);
	} else {
		bytes += qq_put8(raw_data + bytes, auth_len);
		bytes += qq_putdata(raw_data + bytes, auth, auth_len);
	}
	bytes += qq_put8(raw_data + bytes, 1);	/* ALLOW ADD ME FLAG */
	bytes += qq_put8(raw_data + bytes, 0);	/* Destination group */
	if (text == NULL) {
		bytes += qq_put8(raw_data + bytes, 0);
	} else {
		msg = utf8_to_qq(text, QQ_CHARSET_DEFAULT);
		bytes += qq_put8(raw_data + bytes, strlen(msg));
		bytes += qq_putdata(raw_data + bytes, (guint8 *)msg, strlen(msg));
		g_free(msg);
	}
	qq_send_cmd(gc, QQ_CMD_BUDDY_ADD_AUTH_EX, raw_data, bytes);
}

static void request_buddy_add_auth_cb(qq_buddy_req *add_req, const gchar *text)
{
	qq_data *qd;
	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->uid == 0) {
		buddy_add_req_free(add_req);
		return;
	}

	qd = (qq_data *)add_req->gc->proto_data;
	if (qd->client_version > 2005) {
		request_buddy_auth_2007(add_req->gc, add_req->uid,
				text, add_req->auth, add_req->auth_len);
	} else {
		request_buddy_auth(add_req->gc, add_req->uid, QQ_MY_AUTH_REQUEST, text);
	}
	buddy_add_req_free(add_req);
}

/* the real packet to reject and request is sent from here */
static void buddy_add_deny_reason_cb(qq_buddy_req *add_req, const gchar *reason)
{
	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->uid == 0) {
		buddy_add_req_free(add_req);
		return;
	}

	request_buddy_auth(add_req->gc, add_req->uid, QQ_MY_AUTH_REJECT, reason);
	buddy_add_req_free(add_req);
}

static void buddy_add_deny_noreason_cb(qq_buddy_req *add_req)
{
	buddy_add_deny_reason_cb(add_req, NULL);
}

/* we approve other's request of adding me as friend */
static void buddy_add_authorize_cb(gpointer data)
{
	qq_buddy_req *add_req = (qq_buddy_req *)data;

	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->uid == 0) {
		buddy_add_req_free(add_req);
		return;
	}

	request_buddy_auth(add_req->gc, add_req->uid, QQ_MY_AUTH_APPROVE, NULL);
	buddy_add_req_free(add_req);
}

/* we reject other's request of adding me as friend */
static void buddy_add_deny_cb(gpointer data)
{
	qq_buddy_req *add_req = (qq_buddy_req *)data;
	gchar *who = uid_to_purple_name(add_req->uid);
	purple_request_input(add_req->gc, NULL, _("Authorization denied message:"),
			NULL, _("Sorry, You are not my style."), TRUE, FALSE, NULL,
			_("OK"), G_CALLBACK(buddy_add_deny_reason_cb),
			_("Cancel"), G_CALLBACK(buddy_add_deny_noreason_cb),
			purple_connection_get_account(add_req->gc), who, NULL,
			add_req);
	g_free(who);
}

static void buddy_cancel_cb(qq_buddy_req *add_req, const gchar *msg)
{
	g_return_if_fail(add_req != NULL);
	buddy_add_req_free(add_req);
}

static void buddy_add_no_auth_cb(qq_buddy_req *add_req)
{
	qq_data *qd;
	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->uid == 0) {
		buddy_add_req_free(add_req);
		return;
	}

	qd = (qq_data *) add_req->gc->proto_data;
	if (qd->client_version > 2005) {
		request_buddy_add_no_auth_ex(add_req->gc, add_req->uid);
	} else {
		request_buddy_add_no_auth(add_req->gc, add_req->uid);
	}
	buddy_add_req_free(add_req);
}

void buddy_add_authorize_input(PurpleConnection *gc, guint32 uid,
		guint8 *auth, guint8 auth_len)
{
	gchar *who, *msg;
	qq_buddy_req *add_req;
	g_return_if_fail(uid != 0);

	add_req = g_new0(qq_buddy_req, 1);
	add_req->gc = gc;
	add_req->uid = uid;
	add_req->auth = NULL;
	add_req->auth_len = 0;
	if (auth != NULL && auth_len > 0) {
		add_req->auth = g_new0(guint8, auth_len);
		g_memmove(add_req->auth, auth, auth_len);
		add_req->auth_len = auth_len;
	}

	who = uid_to_purple_name(uid);
	msg = g_strdup_printf(_("%d needs authentication"), uid);
	purple_request_input(gc, _("Add buddy authorize"), msg,
			_("Input request here"),
			_("Would you be my friend?"),
			TRUE, FALSE, NULL,
			_("Send"), G_CALLBACK(request_buddy_add_auth_cb),
			_("Cancel"), G_CALLBACK(buddy_cancel_cb),
			purple_connection_get_account(gc), who, NULL,
			add_req);

	g_free(msg);
	g_free(who);
}

/* add a buddy and send packet to QQ server
 * note that when purple load local cached buddy list into its blist
 * it also calls this funtion, so we have to
 * define qd->is_login=TRUE AFTER LOGIN */
void qq_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	qq_data *qd;
	guint32 uid;

	g_return_if_fail(NULL != gc && NULL != gc->proto_data);
	g_return_if_fail(buddy != NULL);

	qd = (qq_data *) gc->proto_data;
	if (!qd->is_login)
		return;		/* IMPORTANT ! */

	uid = purple_name_to_uid(buddy->name);
	if (uid > 0) {
		if (qd->client_version > 2005) {
			request_buddy_add_no_auth_ex(gc, uid);
		} else {
			request_buddy_add_no_auth(gc, uid);
		}
		return;
	}

	purple_notify_error(gc, _("QQ Buddy"), _("Add buddy"), _("Invalid QQ Number"));
	if (buddy == NULL) {
		return;
	}

	purple_debug_info("QQ", "Remove buddy with invalid QQ number %d\n", uid);
	qq_buddy_free(buddy);
}

/*  process reply to add_buddy_auth request */
void qq_process_buddy_add_auth(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	gchar **segments, *msg_utf8;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	if (data[0] == '0') {
		purple_debug_info("QQ", "Reply OK for sending authorize\n");
		return;
	}

	if (NULL == (segments = split_data(data, data_len, "\x1f", 2))) {
		purple_notify_error(gc, _("QQ Buddy"), _("Failed sending authorize"), NULL);
		return;
	}
	msg_utf8 = qq_to_utf8(segments[1], QQ_CHARSET_DEFAULT);
	purple_notify_error(gc, _("QQ Buddy"), _("Failed sending authorize"), msg_utf8);
	g_free(msg_utf8);
}

/* process the server reply for my request to remove a buddy */
void qq_process_buddy_remove(PurpleConnection *gc, guint8 *data, gint data_len, guint32 uid)
{
	PurpleBuddy *buddy = NULL;
	gchar *msg;

	g_return_if_fail(data != NULL && data_len != 0);
	g_return_if_fail(uid != 0);

	buddy = qq_buddy_find(gc, uid);
	if (data[0] != 0) {
		msg = g_strdup_printf(_("Failed removing buddy %d"), uid);
		purple_notify_info(gc, _("QQ Buddy"), msg, NULL);
		g_free(msg);
	}

	purple_debug_info("QQ", "Reply OK for removing buddy\n");
	/* remove buddy again */
	if (buddy != NULL) {
		qq_buddy_free(buddy);
	}
}

/* process the server reply for my request to remove myself from a buddy */
void qq_process_buddy_remove_me(PurpleConnection *gc, guint8 *data, gint data_len, guint32 uid)
{
	qq_data *qd;
	gchar *msg;

	g_return_if_fail(data != NULL && data_len != 0);
	qd = (qq_data *) gc->proto_data;

	if (data[0] == 0) {
		purple_debug_info("QQ", "Reply OK for removing me from %d's buddy list\n", uid);
		return;
	}
	msg = g_strdup_printf(_("Failed removing me from %d's buddy list"), uid);
	purple_notify_info(gc, _("QQ Buddy"), msg, NULL);
	g_free(msg);
}

void qq_process_buddy_add_no_auth(PurpleConnection *gc,
		guint8 *data, gint data_len, guint32 uid)
{
	qq_data *qd;
	gchar **segments;
	gchar *dest_uid, *reply;
	PurpleBuddy *buddy;

	g_return_if_fail(data != NULL && data_len != 0);
	g_return_if_fail(uid != 0);

	qd = (qq_data *) gc->proto_data;

	purple_debug_info("QQ", "Process buddy add for id [%d]\n", uid);
	qq_show_packet("buddy_add_no_auth", data, data_len);

	if (NULL == (segments = split_data(data, data_len, "\x1f", 2)))
		return;

	dest_uid = segments[0];
	reply = segments[1];
	if (strtol(dest_uid, NULL, 10) != qd->uid) {	/* should not happen */
		purple_debug_error("QQ", "Add buddy reply is to [%s], not me!", dest_uid);
		g_strfreev(segments);
		return;
	}

	if (strtol(reply, NULL, 10) == 0) {
		/* add OK */
		qq_buddy_find_or_new(gc, uid);

		qq_request_buddy_info(gc, uid, 0, 0);
		if (qd->client_version >= 2007) {
			qq_request_get_level_2007(gc, uid);
		} else {
			qq_request_get_level(gc, uid);
		}
		qq_request_get_buddies_online(gc, 0, 0);

		purple_debug_info("QQ", "Successed adding into %d's buddy list", uid);
		g_strfreev(segments);
		return;
	}

	/* need auth */
	purple_debug_warning("QQ", "Failed adding buddy, need authorize\n");

	buddy = qq_buddy_find(gc, uid);
	if (buddy == NULL) {
		buddy = qq_buddy_new(gc, uid);
	}
	if (buddy != NULL && buddy->proto_data != NULL) {
		/* Not authorized now, free buddy data */
		qq_buddy_data_free(buddy->proto_data);
		buddy->proto_data = NULL;
	}

	buddy_add_authorize_input(gc, uid, NULL, 0);
	g_strfreev(segments);
}

void qq_process_buddy_add_no_auth_ex(PurpleConnection *gc,
		guint8 *data, gint data_len, guint32 uid)
{
	qq_data *qd;
	gint bytes;
	guint32 dest_uid;
	guint8 reply;

	g_return_if_fail(data != NULL && data_len >= 5);
	g_return_if_fail(uid != 0);

	qd = (qq_data *) gc->proto_data;

	purple_debug_info("QQ", "Process buddy add no auth for id [%d]\n", uid);
	qq_show_packet("buddy_add_no_auth_2007", data, data_len);

	bytes = 0;
	bytes += qq_get32(&dest_uid, data + bytes);
	bytes += qq_get8(&reply, data + bytes);

	g_return_if_fail(dest_uid == qd->uid);

	if (reply == 0) {
		/* need auth */
		purple_debug_warning("QQ", "Failed adding buddy, need authorize\n");
		qq_request_auth_info(gc, QQ_AUTH_INFO_BUDDY, QQ_AUTH_INFO_ADD_BUDDY, uid);
		return;
	}

	/* add OK */
	qq_buddy_find_or_new(gc, uid);

	qq_request_buddy_info(gc, uid, 0, 0);
	if (qd->client_version >= 2007) {
		qq_request_get_level_2007(gc, uid);
	} else {
		qq_request_get_level(gc, uid);
	}
	qq_request_get_buddies_online(gc, 0, 0);

	purple_debug_info("QQ", "Successed adding into %d's buddy list", uid);
	return;
}

/* remove a buddy and send packet to QQ server accordingly */
void qq_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	qq_data *qd;
	guint32 uid;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(buddy != NULL);

	qd = (qq_data *) gc->proto_data;
	if (!qd->is_login)
		return;

	uid = purple_name_to_uid(buddy->name);
	if (uid > 0 && uid != qd->uid) {
		if (qd->client_version > 2005) {
			qq_request_auth_info(gc, QQ_AUTH_INFO_BUDDY, QQ_AUTH_INFO_REMOVE_BUDDY, uid);
		} else {
			request_buddy_remove(gc, uid);
			request_buddy_remove_me(gc, uid);
		}
	}

	if (buddy->proto_data) {
		qq_buddy_data_free(buddy->proto_data);
		buddy->proto_data = NULL;
	} else {
		purple_debug_warning("QQ", "Empty buddy data of %s\n", buddy->name);
	}

	/* Do not call purple_blist_remove_buddy,
	 * otherwise purple segmentation fault */
}

static void buddy_add_request(PurpleConnection *gc, guint32 uid, gchar *reason)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	qq_buddy_req *add_req;
	gchar *who;

	g_return_if_fail(uid != 0 && reason != NULL);

	purple_debug_info("QQ", "Buddy %d request adding, msg: %s\n", uid, reason);

	add_req = g_new0(qq_buddy_req, 1);
	add_req->gc = gc;
	add_req->uid = uid;

	if (purple_prefs_get_bool("/plugins/prpl/qq/auto_get_authorize_info")) {
		qq_request_buddy_info(gc, add_req->uid, 0, QQ_BUDDY_INFO_DISPLAY);
	}
	who = uid_to_purple_name(add_req->uid);

	purple_account_request_authorization(account,
	 		who, NULL,
			NULL, reason,
			purple_find_buddy(account, who) != NULL,
			buddy_add_authorize_cb,
			buddy_add_deny_cb,
			add_req);

	g_free(who);
}

/* someone wants to add you to his buddy list */
static void server_buddy_add_request(PurpleConnection *gc, gchar *from, gchar *to,
		guint8 *data, gint data_len)
{
	guint32 uid;
	gchar *msg, *reason;

	g_return_if_fail(from != NULL && to != NULL);
	uid = strtol(from, NULL, 10);
	g_return_if_fail(uid != 0);

	if (purple_prefs_get_bool("/plugins/prpl/qq/auto_get_authorize_info")) {
		qq_request_buddy_info(gc, uid, 0, QQ_BUDDY_INFO_DISPLAY);
	}

	if (data_len <= 0) {
		reason = g_strdup( _("No reason given") );
	} else {
		msg = g_strndup((gchar *)data, data_len);
		reason = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
		if (reason == NULL)	reason = g_strdup( _("Unknown reason") );
		g_free(msg);
	}

	buddy_add_request(gc, uid, reason);
	g_free(reason);
}

void qq_process_buddy_check_code(PurpleConnection *gc, guint8 *data, gint data_len, guint32 ship32)
{
	qq_data *qd;
	gint bytes;
	guint8 cmd;
	guint8 reply;
	guint32 uid;
	guint16 flag1, flag2;

	g_return_if_fail(data != NULL && data_len >= 5);
	g_return_if_fail(uid != 0);

	qd = (qq_data *) gc->proto_data;

	qq_show_packet("buddy_check_code", data, data_len);

	bytes = 0;
	bytes += qq_get8(&cmd, data + bytes);		/* 0x03 */
	bytes += qq_get8(&reply, data + bytes);

	if (reply == 0) {
		purple_debug_info("QQ", "Failed checking code\n");
		return;
	}

	bytes += qq_get32(&uid, data + bytes);
	g_return_if_fail(uid != 0);
	bytes += qq_get16(&flag1, data + bytes);
	bytes += qq_get16(&flag2, data + bytes);
	purple_debug_info("QQ", "Check code reply Ok, uid %d, flag 0x%04X-0x%04X\n",
			uid, flag1, flag2);
	return;
}

static void request_buddy_check_code(PurpleConnection *gc,
		gchar *from, guint8 *code, gint code_len)
{
	guint8 *raw_data;
	gint bytes;
	guint32 uid;

	g_return_if_fail(code != NULL && code_len > 0 && from != NULL);

	uid = strtol(from, NULL, 10);
	raw_data = g_newa(guint8, code_len + 16);
	bytes = 0;
	bytes += qq_put8(raw_data + bytes, 0x03);
	bytes += qq_put8(raw_data + bytes, 0x01);
	bytes += qq_put32(raw_data + bytes, uid);
	bytes += qq_put16(raw_data + bytes, code_len);
	bytes += qq_putdata(raw_data + bytes, code, code_len);

	qq_send_cmd(gc, QQ_CMD_BUDDY_CHECK_CODE, raw_data, bytes);
}

static gint server_buddy_check_code(PurpleConnection *gc,
		gchar *from, guint8 *data, gint data_len)
{
	gint bytes;
	guint8 *code;
	guint16 code_len;

	g_return_val_if_fail(data != NULL && data_len > 0, 0);

	bytes = 0;
	bytes += qq_get16(&code_len, data + bytes);
	if (code_len <= 0) {
		purple_debug_info("QQ", "Server msg for buddy has no code\n");
		return bytes;
	}
	if (bytes + code_len < data_len) {
		purple_debug_error("QQ", "Code len error in server msg for buddy\n");
		qq_show_packet("server_buddy_check_code", data, data_len);
		code_len = data_len - bytes;
	}
	code = g_newa(guint8, code_len);
	bytes += qq_getdata(code, code_len, data + bytes);

	request_buddy_check_code(gc, from, code, code_len);
	return bytes;
}

static void server_buddy_add_request_ex(PurpleConnection *gc, gchar *from, gchar *to,
		guint8 *data, gint data_len)
{
	gint bytes;
	guint32 uid;
	gchar *msg;
	guint8 allow_reverse;

	g_return_if_fail(from != NULL && to != NULL);
	g_return_if_fail(data != NULL && data_len >= 3);
	uid = strtol(from, NULL, 10);
	g_return_if_fail(uid != 0);

	/* qq_show_packet("server_buddy_add_request_ex", data, data_len); */

	bytes = 0;
	bytes += qq_get_vstr(&msg, QQ_CHARSET_DEFAULT, data+bytes);
	bytes += qq_get8(&allow_reverse, data + bytes);	/* allow_reverse = 0x01, allowed */
	server_buddy_check_code(gc, from, data + bytes, data_len - bytes);

	if (strlen(msg) <= 0) {
		g_free(msg);
		msg = g_strdup( _("No reason given") );
	}
	buddy_add_request(gc, uid, msg);
	g_free(msg);
}

/* when you are added by a person, QQ server will send sys message */
static void server_buddy_added(PurpleConnection *gc, gchar *from, gchar *to,
		guint8 *data, gint data_len)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleBuddy *buddy;
	guint32 uid;
	qq_buddy_req *add_req;
	gchar *who;
	gchar *primary;

	g_return_if_fail(from != NULL && to != NULL);

	uid = strtol(from, NULL, 10);
	who = uid_to_purple_name(uid);

	buddy = purple_find_buddy(account, who);
	if (buddy != NULL) {
		purple_account_notify_added(account, from, to, NULL, NULL);
	}

	add_req = g_new0(qq_buddy_req, 1);
	add_req->gc = gc;
	add_req->uid = uid;	/* only need to get value */
	primary = g_strdup_printf(_("You have been added by %s"), from);
	purple_request_action(gc, NULL, primary,
			_("Would you like to add him?"),
			PURPLE_DEFAULT_ACTION_NONE,
			purple_connection_get_account(gc), who, NULL,
			add_req, 2,
			_("Cancel"), G_CALLBACK(buddy_cancel_cb),
			_("Add"), G_CALLBACK(buddy_add_no_auth_cb));

	g_free(who);
	g_free(primary);
}

static void server_buddy_added_ex(PurpleConnection *gc, gchar *from, gchar *to,
		guint8 *data, gint data_len)
{
	gint bytes;
	guint8 allow_reverse;
	gchar *msg;

	g_return_if_fail(from != NULL && to != NULL);
	g_return_if_fail(data != NULL && data_len >= 3);

	qq_show_packet("server_buddy_added_ex", data, data_len);

	bytes = 0;
	bytes += qq_get_vstr(&msg, QQ_CHARSET_DEFAULT, data+bytes);	/* always empty msg */
	purple_debug_info("QQ", "Buddy added msg: %s\n", msg);
	bytes += qq_get8(&allow_reverse, data + bytes);	/* allow_reverse = 0x01, allowed */
	server_buddy_check_code(gc, from, data + bytes, data_len - bytes);

	g_free(msg);
}

static void server_buddy_adding_ex(PurpleConnection *gc, gchar *from, gchar *to,
		guint8 *data, gint data_len)
{
	gint bytes;
	guint8 allow_reverse;

	g_return_if_fail(from != NULL && to != NULL);
	g_return_if_fail(data != NULL && data_len >= 3);

	qq_show_packet("server_buddy_adding_ex", data, data_len);

	bytes = 0;
	bytes += qq_get8(&allow_reverse, data + bytes);	/* allow_reverse = 0x01, allowed */
	server_buddy_check_code(gc, from, data + bytes, data_len - bytes);
}

/* the buddy approves your request of adding him/her as your friend */
static void server_buddy_added_me(PurpleConnection *gc, gchar *from, gchar *to,
		guint8 *data, gint data_len)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	qq_data *qd;
	guint32 uid;

	g_return_if_fail(from != NULL && to != NULL);

	qd = (qq_data *) gc->proto_data;

	uid = strtol(from, NULL, 10);
	g_return_if_fail(uid > 0);

	server_buddy_check_code(gc, from, data, data_len);

	qq_buddy_find_or_new(gc, uid);
	qq_request_buddy_info(gc, uid, 0, 0);
	qq_request_get_buddies_online(gc, 0, 0);
	if (qd->client_version >= 2007) {
		qq_request_get_level_2007(gc, uid);
	} else {
		qq_request_get_level(gc, uid);
	}

	purple_account_notify_added(account, to, from, NULL, NULL);
}

/* you are rejected by the person */
static void server_buddy_rejected_me(PurpleConnection *gc, gchar *from, gchar *to,
		guint8 *data, gint data_len)
{
	guint32 uid;
	PurpleBuddy *buddy;
	gchar *msg, *msg_utf8;
	gint bytes;
	gchar **segments;
	gchar *primary, *secondary;

	g_return_if_fail(from != NULL && to != NULL);

	qq_show_packet("server_buddy_rejected_me", data, data_len);

	if (data_len <= 0) {
		msg = g_strdup( _("No reason given") );
	} else {
		segments = g_strsplit((gchar *)data, "\x1f", 1);
		if (segments != NULL && segments[0] != NULL) {
			msg = g_strdup(segments[0]);
			g_strfreev(segments);
			bytes = strlen(msg) + 1;
			if (bytes < data_len) {
				server_buddy_check_code(gc, from, data + bytes, data_len - bytes);
			}
		} else {
			msg = g_strdup( _("No reason given") );
		}
	}
	msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
	if (msg_utf8 == NULL) {
		msg_utf8 = g_strdup( _("Unknown reason") );
	}
	g_free(msg);

	primary = g_strdup_printf(_("Rejected by %s"), from);
	secondary = g_strdup_printf(_("Message: %s"), msg_utf8);

	purple_notify_info(gc, _("QQ Buddy"), primary, secondary);

	g_free(msg_utf8);
	g_free(primary);
	g_free(secondary);

	uid = strtol(from, NULL, 10);
	g_return_if_fail(uid != 0);

	buddy = qq_buddy_find(gc, uid);
	if (buddy != NULL && buddy->proto_data != NULL) {
		/* Not authorized now, free buddy data */
		qq_buddy_data_free(buddy->proto_data);
		buddy->proto_data = NULL;
	}
}

void qq_process_buddy_from_server(PurpleConnection *gc, int funct,
		gchar *from, gchar *to, guint8 *data, gint data_len)
{
	switch (funct) {
	case QQ_SERVER_BUDDY_ADDED:
		server_buddy_added(gc, from, to, data, data_len);
		break;
	case QQ_SERVER_BUDDY_ADD_REQUEST:
		server_buddy_add_request(gc, from, to, data, data_len);
		break;
	case QQ_SERVER_BUDDY_ADD_REQUEST_EX:
		server_buddy_add_request_ex(gc, from, to, data, data_len);
		break;
	case QQ_SERVER_BUDDY_ADDED_ME:
		server_buddy_added_me(gc, from, to, data, data_len);
		break;
	case QQ_SERVER_BUDDY_REJECTED_ME:
		server_buddy_rejected_me(gc, from, to, data, data_len);
		break;
	case QQ_SERVER_BUDDY_ADDED_EX:
		server_buddy_added_ex(gc, from, to, data, data_len);
		break;
	case QQ_SERVER_BUDDY_ADDING_EX:
	case QQ_SERVER_BUDDY_ADDED_ANSWER:
		server_buddy_adding_ex(gc, from, to, data, data_len);
		break;
	default:
		purple_debug_warning("QQ", "Unknow buddy operate (%d) from server\n", funct);
		break;
	}
}
