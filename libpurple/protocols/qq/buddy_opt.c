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
#define PURPLE_GROUP_QQ_UNKNOWN      "QQ Unknown"

#define QQ_REMOVE_BUDDY_REPLY_OK      0x00
#define QQ_REMOVE_SELF_REPLY_OK       0x00
#define QQ_ADD_BUDDY_AUTH_REPLY_OK    0x30	/* ASCII value of "0" */

enum {
	QQ_MY_AUTH_APPROVE = 0x30,	/* ASCII value of "0" */
	QQ_MY_AUTH_REJECT = 0x31,	/* ASCII value of "1" */
	QQ_MY_AUTH_REQUEST = 0x32,	/* ASCII value of "2" */
};

typedef struct _qq_buddy_req {
	guint32 uid;
	PurpleConnection *gc;
} qq_buddy_req;

/* send packet to remove a buddy from my buddy list */
static void request_buddy_remove(PurpleConnection *gc, guint32 uid)
{
	gchar uid_str[11];

	g_return_if_fail(uid > 0);

	g_snprintf(uid_str, sizeof(uid_str), "%d", uid);
	qq_send_cmd(gc, QQ_CMD_BUDDY_REMOVE, (guint8 *) uid_str, strlen(uid_str));
}

/* try to remove myself from someone's buddy list */
static void request_buddy_remove_me(PurpleConnection *gc, guint32 uid)
{
	guint8 raw_data[16] = {0};
	gint bytes = 0;

	g_return_if_fail(uid > 0);

	bytes += qq_put32(raw_data + bytes, uid);

	qq_send_cmd(gc, QQ_CMD_REMOVE_ME, raw_data, bytes);
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

/* we approve other's request of adding me as friend */
static void buddy_add_authorize_cb(qq_buddy_req *add_req)
{
	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->uid != 0) {
		g_free(add_req);
		return;
	}

	request_buddy_auth(add_req->gc, add_req->uid, QQ_MY_AUTH_APPROVE, NULL);
	g_free(add_req);
}

/* we reject other's request of adding me as friend */
static void buddy_add_deny_cb(qq_buddy_req *add_req)
{
	gint uid;
	gchar *msg1, *msg2;
	PurpleConnection *gc;
	gchar *purple_name;

	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->uid == 0) {
		g_free(add_req);
		return;
	}

	gc = add_req->gc;
	uid = add_req->uid;

	msg1 = g_strdup_printf(_("You rejected %d's request"), uid);
	msg2 = g_strdup(_("Message:"));

	purple_name = uid_to_purple_name(uid);
	purple_request_input(gc, _("Reject request"), msg1, msg2,
			_("Sorry, you are not my style..."), TRUE, FALSE,
			NULL, _("Reject"), G_CALLBACK(buddy_add_deny_reason_cb), _("Cancel"), NULL,
			purple_connection_get_account(gc), purple_name, NULL,
			add_req);
	g_free(purple_name);
}

/* suggested by rakescar@linuxsir, can still approve after search */
static void buddy_add_check_info_cb(qq_buddy_req *add_req)
{
	PurpleConnection *gc;
	guint32 uid;
	gchar *purple_name;

	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->uid == 0) {
		g_free(add_req);
		return;
	}

	gc = add_req->gc;
	uid = add_req->uid;

	qq_request_buddy_info(gc, uid, 0, QQ_BUDDY_INFO_DISPLAY);

	purple_name = uid_to_purple_name(uid);
	purple_request_action
	    (gc, NULL, _("Do you approve the requestion?"), "",
		PURPLE_DEFAULT_ACTION_NONE,
		 purple_connection_get_account(gc), purple_name, NULL,
		 add_req, 2,
	     _("Reject"), G_CALLBACK(buddy_add_deny_cb),
	     _("Approve"), G_CALLBACK(buddy_add_authorize_cb));
	g_free(purple_name);
}

/* add a buddy and send packet to QQ server
 * note that when purple load local cached buddy list into its blist
 * it also calls this funtion, so we have to
 * define qd->is_login=TRUE AFTER serv_finish_login(gc) */
void qq_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	qq_data *qd;
	guint32 uid;
	PurpleBuddy *b;

	qd = (qq_data *) gc->proto_data;
	if (!qd->is_login)
		return;		/* IMPORTANT ! */

	uid = purple_name_to_uid(buddy->name);
	if (uid > 0) {
		request_buddy_add_no_auth(gc, uid);
		return;
	}

	b = purple_find_buddy(gc->account, buddy->name);
	if (b != NULL) {
		purple_blist_remove_buddy(b);
	}
	purple_notify_error(gc, _("QQ Buddy"), _("QQ Number Error"), _("Invalid QQ Number"));
}

void qq_change_buddys_group(PurpleConnection *gc, const char *who,
		const char *old_group, const char *new_group)
{
	gint uid;
	g_return_if_fail(who != NULL);

	if (strcmp(new_group, PURPLE_GROUP_QQ_UNKNOWN) == 0) {
		if (purple_privacy_check(gc->account, who)) {
			purple_privacy_deny(gc->account, who, TRUE, FALSE);
		} else {
			purple_privacy_deny_add(gc->account, who, TRUE);
		}
		return;
	}

	if (strcmp(old_group, PURPLE_GROUP_QQ_UNKNOWN) != 0) {
		return;
	}

	uid = purple_name_to_uid(who);
	g_return_if_fail(uid != 0);

	purple_privacy_deny_remove(gc->account, who, TRUE);

	purple_debug_info("QQ", "Add unknow buddy %d\n", uid);
	request_buddy_add_no_auth(gc, uid);
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

static void buddy_remove_both_cb(qq_buddy_req *add_req)
{
	PurpleConnection *gc;
	qq_data *qd;
	gchar *purple_name;
	PurpleBuddy *buddy;

	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->uid == 0) {
		g_free(add_req);
		return;
	}

	gc = add_req->gc;
	qd = (qq_data *) gc->proto_data;

	request_buddy_remove(gc, add_req->uid);
	request_buddy_remove_me(gc, add_req->uid);

	purple_name = uid_to_purple_name(add_req->uid);
	buddy = purple_find_buddy(gc->account, purple_name);
	if (buddy == NULL) {
		g_free(add_req);
		return;
	}

	if (buddy->proto_data != NULL) {
		qq_buddy_data_free(buddy->proto_data);
		buddy->proto_data = NULL;
	} else {
		purple_debug_warning("QQ", "We have no qq_buddy_data record for %s\n", buddy->name);
	}

	purple_blist_remove_buddy(buddy);
	g_free(add_req);
}

/* remove a buddy from my list and remove myself from his list */
/* TODO: re-enable this */
void qq_remove_buddy_and_me(PurpleBlistNode * node)
{
	PurpleConnection *gc;
	qq_data *qd;
	guint32 uid;
	qq_buddy_req *add_req;
	PurpleBuddy *buddy;
	const gchar *who;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);
	qd = (qq_data *) gc->proto_data;
	if (!qd->is_login)
		return;

	who = buddy->name;
	g_return_if_fail(who != NULL);

	uid = purple_name_to_uid(who);
	g_return_if_fail(uid > 0);

	if (uid == qd->uid) {
		return;
	}

	add_req = g_new0(qq_buddy_req, 1);
	add_req->gc = gc;
	add_req->uid = uid;

	purple_request_action(gc, _("Block Buddy"),
			    "Are you sure you want to block this buddy?",
			    NULL,
			    1,
				purple_connection_get_account(gc), NULL, NULL,
				add_req, 2,
			    _("Cancel"), G_CALLBACK(buddy_cancel_cb),
				_("Block"), G_CALLBACK(buddy_remove_both_cb));
}

/*  process reply to add_buddy_auth request */
void qq_process_buddy_add_auth(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	gchar **segments, *msg_utf8;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	if (data[0] != QQ_ADD_BUDDY_AUTH_REPLY_OK) {
		purple_debug_warning("QQ", "Failed authorizing of adding requestion\n");
		if (NULL == (segments = split_data(data, data_len, "\x1f", 2))) {
			return;
		}
		msg_utf8 = qq_to_utf8(segments[1], QQ_CHARSET_DEFAULT);
		purple_notify_error(gc, _("QQ Buddy"), _("Failed authorizing of adding requestion"), msg_utf8);
		g_free(msg_utf8);
	} else {
		qq_got_attention(gc, _("Successed authorizing of adding requestion"));
	}
}

/* process the server reply for my request to remove a buddy */
void qq_process_buddy_remove(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	if (data[0] != QQ_REMOVE_BUDDY_REPLY_OK) {
		/* there is no reason return from server */
		purple_debug_warning("QQ", "Remove buddy fails\n");
		purple_notify_info(gc, _("QQ Buddy"), _("Failed:"),  _("Remove buddy"));
	} else {		/* if reply */
		qq_got_attention(gc, _("Successed removing budy."));
	}
}

/* process the server reply for my request to remove myself from a buddy */
void qq_process_buddy_remove_me(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	if (data[0] != QQ_REMOVE_SELF_REPLY_OK) {
		/* there is no reason return from server */
		purple_debug_warning("QQ", "Remove self fails\n");
		purple_notify_info(gc, _("QQ Buddy"), _("Failed:"), _("Remove me from other's buddy list"));
	} else {		/* if reply */
		qq_got_attention(gc, _("Successed removing me from other's budy list."));
	}
}

void qq_process_buddy_add_no_auth(guint8 *data, gint data_len, guint32 uid, PurpleConnection *gc)
{
	qq_data *qd;
	gchar *msg, **segments, *dest_uid, *reply;
	PurpleBuddy *b;
	qq_buddy_req *add_req;
	gchar *nombre;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	if (uid == 0) {	/* we have no record for this */
		purple_debug_error("QQ", "Process buddy add, unknow id\n");
		return;
	} else {
		purple_debug_info("QQ", "Process buddy add for id [%d]\n", uid);
	}

	if (NULL == (segments = split_data(data, data_len, "\x1f", 2)))
		return;

	dest_uid = segments[0];
	reply = segments[1];
	if (strtol(dest_uid, NULL, 10) != qd->uid) {	/* should not happen */
		purple_debug_error("QQ", "Add buddy reply is to [%s], not me!", dest_uid);
		g_strfreev(segments);
		return;
	}

	if (strtol(reply, NULL, 10) > 0) {	/* need auth */
		purple_debug_warning("QQ", "Add buddy attempt fails, need authentication\n");
		nombre = uid_to_purple_name(uid);
		b = purple_find_buddy(gc->account, nombre);
		if (b != NULL)
			purple_blist_remove_buddy(b);
		add_req = g_new0(qq_buddy_req, 1);
		add_req->gc = gc;
		add_req->uid = uid;
		msg = g_strdup_printf(_("%d needs authentication"), uid);
		purple_request_input(gc, NULL, msg,
				_("Input request here"), /* TODO: Awkward string to fix post string freeze - standardize auth dialogues? -evands */
				_("Would you be my friend?"),
				TRUE, FALSE, NULL, _("Send"),
				G_CALLBACK(request_buddy_add_auth_cb),
				_("Cancel"), G_CALLBACK(buddy_cancel_cb),
				purple_connection_get_account(gc), nombre, NULL,
				add_req);
		g_free(msg);
		g_free(nombre);
	} else {	/* add OK */
		qq_buddy_find_or_new(gc, uid);
		qq_request_buddy_info(gc, uid, 0, 0);
		qq_request_get_buddies_online(gc, 0, 0);
		if (qd->client_version >= 2007) {
			qq_request_get_level_2007(gc, uid);
		} else {
			qq_request_get_level(gc, uid);
		}

		msg = g_strdup_printf(_("Successed adding into %d's buddy list"), uid);
		qq_got_attention(gc, msg);
		g_free(msg);
	}
	g_strfreev(segments);
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
	bd->status = QQ_BUDDY_ONLINE_NORMAL;
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
	buddy = purple_buddy_new(gc->account, who, NULL);	/* alias is NULL */
	g_free(who);
	buddy->proto_data = NULL;

	purple_blist_add_buddy(buddy, NULL, group, NULL);
	purple_debug_info("QQ", "Add new purple buddy: [%s]\n", who);

	g_free(group_name);

	return buddy;
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

/* remove a buddy and send packet to QQ server accordingly */
void qq_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	qq_data *qd;
	guint32 uid;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(buddy != NULL);
	qd = (qq_data *) gc->proto_data;

	uid = purple_name_to_uid(buddy->name);
	if (!qd->is_login)
		return;

	if (uid > 0) {
		request_buddy_remove(gc, uid);
	}

	if (buddy->proto_data) {
		qq_buddy_data_free(buddy->proto_data);
		buddy->proto_data = NULL;
	} else {
		purple_debug_warning("QQ", "We have no qq_buddy_data record for %s\n", buddy->name);
	}

	/* Do not call purple_blist_remove_buddy,
	 * otherwise purple segmentation fault */
}

/* someone wants to add you to his buddy list */
static void server_buddy_add_request(PurpleConnection *gc, gchar *from, gchar *to, gchar *msg_utf8)
{
	gchar *message, *reason;
	guint32 uid;
	qq_buddy_req *g, *g2;
	PurpleBuddy *b;
	gchar *name;

	g_return_if_fail(from != NULL && to != NULL);

	uid = strtol(from, NULL, 10);
	g = g_new0(qq_buddy_req, 1);
	g->gc = gc;
	g->uid = uid;

	name = uid_to_purple_name(uid);

	/* TODO: this should go through purple_account_request_authorization() */
	message = g_strdup_printf(_("%s wants to add you [%s] as a friend"), from, to);
	reason = g_strdup_printf(_("Message: %s"), msg_utf8);

	purple_request_action
	    (gc, NULL, message, reason, PURPLE_DEFAULT_ACTION_NONE,
		purple_connection_get_account(gc), name, NULL,
		 g, 3,
	     _("Reject"),
	     G_CALLBACK(buddy_add_deny_cb),
	     _("Approve"),
	     G_CALLBACK(buddy_add_authorize_cb),
	     _("Search"), G_CALLBACK(buddy_add_check_info_cb));

	g_free(message);
	g_free(reason);

	/* XXX: Is this needed once the above goes through purple_account_request_authorization()? */
	b = purple_find_buddy(gc->account, name);
	if (b == NULL) {	/* the person is not in my list  */
		g2 = g_new0(qq_buddy_req, 1);
		g2->gc = gc;
		g2->uid = strtol(from, NULL, 10);
		message = g_strdup_printf(_("%s is not in buddy list"), from);
		purple_request_action(gc, NULL, message,
				    _("Would you add?"), PURPLE_DEFAULT_ACTION_NONE,
					purple_connection_get_account(gc), name, NULL,
					g2, 3,
					_("Cancel"), NULL,
					_("Add"), G_CALLBACK(buddy_add_no_auth_cb),
				    _("Search"), G_CALLBACK(buddy_add_check_info_cb));
		g_free(message);
	}

	g_free(name);
}

/* when you are added by a person, QQ server will send sys message */
static void server_buddy_added(PurpleConnection *gc, gchar *from, gchar *to, gchar *msg_utf8)
{
	gchar *message;
	PurpleBuddy *b;
	guint32 uid;
	qq_buddy_req *add_req;
	gchar *name;

	g_return_if_fail(from != NULL && to != NULL);

	uid = strtol(from, NULL, 10);
	name = uid_to_purple_name(uid);
	b = purple_find_buddy(gc->account, name);

	if (b == NULL) {	/* the person is not in my list */
		add_req = g_new0(qq_buddy_req, 1);
		add_req->gc = gc;
		add_req->uid = uid;	/* only need to get value */
		message = g_strdup_printf(_("You have been added by %s"), from);
		purple_request_action(gc, NULL, message,
				    _("Would you like to add him?"),
					PURPLE_DEFAULT_ACTION_NONE,
					purple_connection_get_account(gc), name, NULL,
					add_req, 3,
				    _("Cancel"), G_CALLBACK(buddy_cancel_cb),
					_("Add"), G_CALLBACK(buddy_add_no_auth_cb),
				    _("Search"), G_CALLBACK(buddy_add_check_info_cb));
	} else {
		message = g_strdup_printf(_("Successed adding into %s's buddy list"), from);
		qq_got_attention(gc, message);
	}

	g_free(name);
	g_free(message);
}

/* the buddy approves your request of adding him/her as your friend */
static void server_buddy_added_me(PurpleConnection *gc, gchar *from, gchar *to, gchar *msg_utf8)
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

	purple_account_notify_added(account, from, to, NULL, msg_utf8);
}

/* you are rejected by the person */
static void server_buddy_rejected_me(PurpleConnection *gc, gchar *from, gchar *to, gchar *msg_utf8)
{
	gchar *message, *reason;

	g_return_if_fail(from != NULL && to != NULL);

	message = g_strdup_printf(_("Requestion rejected by %s"), from);
	reason = g_strdup_printf(_("Message: %s"), msg_utf8);

	purple_notify_info(gc, _("QQ Buddy"), message, reason);
	g_free(message);
	g_free(reason);
}

void qq_process_buddy_from_server(PurpleConnection *gc, int funct,
		gchar *from, gchar *to, gchar *msg_utf8)
{
	switch (funct) {
	case QQ_SERVER_BUDDY_ADDED:
		server_buddy_added(gc, from, to, msg_utf8);
		break;
	case QQ_SERVER_BUDDY_ADD_REQUEST:
		server_buddy_add_request(gc, from, to, msg_utf8);
		break;
	case QQ_SERVER_BUDDY_ADDED_ME:
		server_buddy_added_me(gc, from, to, msg_utf8);
		break;
	case QQ_SERVER_BUDDY_REJECTED_ME:
		server_buddy_rejected_me(gc, from, to, msg_utf8);
		break;
	default:
		purple_debug_warning("QQ", "Unknow buddy operate (%d) from server\n", funct);
		break;
	}
}
