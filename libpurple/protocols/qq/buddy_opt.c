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
	guint32 uid;
	PurpleConnection *gc;
} qq_buddy_req;

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
	}
	if (cmd == QQ_AUTH_INFO_BUDDY && sub_cmd == QQ_AUTH_INFO_ADD_BUDDY) {
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

static void request_buddy_add_auth_cb(qq_buddy_req *add_req, const gchar *text)
{
	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->uid == 0) {
		g_free(add_req);
		return;
	}

	request_buddy_auth(add_req->gc, add_req->uid, QQ_MY_AUTH_REQUEST, text);
	g_free(add_req);
}

/* the real packet to reject and request is sent from here */
static void buddy_add_deny_reason_cb(qq_buddy_req *add_req, const gchar *reason)
{
	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->uid == 0) {
		g_free(add_req);
		return;
	}

	request_buddy_auth(add_req->gc, add_req->uid, QQ_MY_AUTH_REJECT, reason);
	g_free(add_req);
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
		g_free(add_req);
		return;
	}

	request_buddy_auth(add_req->gc, add_req->uid, QQ_MY_AUTH_APPROVE, NULL);
	g_free(add_req);
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
		request_buddy_add_no_auth(gc, uid);
		return;
	}

	purple_notify_error(gc, _("QQ Buddy"), _("Add buddy"), _("Invalid QQ Number"));
	if (buddy == NULL) {
		return;
	}

	purple_debug_info("QQ", "Remove buddy with invalid QQ number %d\n", uid);
	qq_buddy_free(buddy);
}

static void buddy_cancel_cb(qq_buddy_req *add_req, const gchar *msg)
{
	g_return_if_fail(add_req != NULL);
	g_free(add_req);
}

static void buddy_add_no_auth_cb(qq_buddy_req *add_req)
{
	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->uid == 0) {
		g_free(add_req);
		return;
	}

	request_buddy_add_no_auth(add_req->gc, add_req->uid);
	g_free(add_req);
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

void qq_process_buddy_add_no_auth(guint8 *data, gint data_len, guint32 uid, PurpleConnection *gc)
{
	qq_data *qd;
	gchar *msg, **segments, *dest_uid, *reply;
	PurpleBuddy *buddy;
	qq_buddy_req *add_req;
	gchar *who;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	if (uid == 0) {
		purple_debug_error("QQ", "Process buddy add, unknow id\n");
		return;
	}
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

	who = uid_to_purple_name(uid);
	add_req = g_new0(qq_buddy_req, 1);
	add_req->gc = gc;
	add_req->uid = uid;
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
	g_strfreev(segments);
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

/* someone wants to add you to his buddy list */
static void server_buddy_add_request(PurpleConnection *gc, gchar *from, gchar *to,
		guint8 *data, gint data_len)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	qq_buddy_req *add_req;
	gchar *who;
	gchar *msg, *reason;

	g_return_if_fail(from != NULL && to != NULL);

	add_req = g_new0(qq_buddy_req, 1);
	add_req->gc = gc;
	add_req->uid = strtol(from, NULL, 10);;

	if (purple_prefs_get_bool("/plugins/prpl/qq/auto_get_authorize_info")) {
		qq_request_buddy_info(gc, add_req->uid, 0, QQ_BUDDY_INFO_DISPLAY);
	}
	who = uid_to_purple_name(add_req->uid);

	if (data_len <= 0) {
		reason = g_strdup( _("No reason given") );
	} else {
		msg = g_strndup((gchar *)data, data_len);
		reason = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
		if (reason == NULL)	reason = g_strdup( _("Unknown reason") );
		g_free(msg);
	}
	purple_account_request_authorization(account,
	 		from, NULL,
			NULL, reason,
			purple_find_buddy(account, who) != NULL,
			buddy_add_authorize_cb,
			buddy_add_deny_cb,
			add_req);

	g_free(reason);
	g_free(who);
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
	gchar *primary, *secondary;

	g_return_if_fail(from != NULL && to != NULL);

	if (data_len <= 0) {
		msg = g_strdup( _("No reason given") );
	} else {
		msg = g_strndup((gchar *)data, data_len);
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
	case QQ_MSG_SYS_ADD_FRIEND_REQUEST_EX:
		// server_buddy_add_request_ex(gc, from, to, data, data_len);
		break;
	case QQ_SERVER_BUDDY_ADDED_ME:
		server_buddy_added_me(gc, from, to, data, data_len);
		break;
	case QQ_SERVER_BUDDY_REJECTED_ME:
		server_buddy_rejected_me(gc, from, to, data, data_len);
		break;
	default:
		purple_debug_warning("QQ", "Unknow buddy operate (%d) from server\n", funct);
		break;
	}
}
