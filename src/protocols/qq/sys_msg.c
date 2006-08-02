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

#include "debug.h"
#include "internal.h"
#include "notify.h"
#include "request.h"

#include "buddy_info.h"
#include "buddy_list.h"
#include "buddy_opt.h"
#include "char_conv.h"
#include "crypt.h"
#include "header_info.h"
#include "packet_parse.h"
#include "qq.h"
#include "send_core.h"
#include "sys_msg.h"
#include "utils.h"

enum {
	QQ_MSG_SYS_BEING_ADDED = 0x01,
	QQ_MSG_SYS_ADD_CONTACT_REQUEST = 0x02,
	QQ_MSG_SYS_ADD_CONTACT_APPROVED = 0x03,
	QQ_MSG_SYS_ADD_CONTACT_REJECTED = 0x04,
	QQ_MSG_SYS_NEW_VERSION = 0x09
};

/* Henry: private function for reading/writing of system log */
static void _qq_sys_msg_log_write(GaimConnection *gc, gchar *msg, gchar *from)
{
	GaimLog *log;
	GaimAccount *account;

	account = gaim_connection_get_account(gc);
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	log = gaim_log_new(GAIM_LOG_IM,
			"systemim",
			account,
			NULL,
			time(NULL),
			NULL
			);
	gaim_log_write(log, GAIM_MESSAGE_SYSTEM, from,
			time(NULL), msg);
	gaim_log_free(log);
}

/* suggested by rakescar@linuxsir, can still approve after search */
static void _qq_search_before_auth_with_gc_and_uid(gc_and_uid *g)
{
	GaimConnection *gc;
	guint32 uid;

	g_return_if_fail(g != NULL);

	gc = g->gc;
	uid = g->uid;
	g_return_if_fail(gc != 0 && uid != 0);

	qq_send_packet_get_info(gc, uid, TRUE);	/* we wanna see window */
	gaim_request_action
	    (gc, NULL, _("Do you wanna approve the request?"), "", 2, g, 2,
	     _("Reject"),
	     G_CALLBACK(qq_reject_add_request_with_gc_and_uid),
	     _("Approve"), G_CALLBACK(qq_approve_add_request_with_gc_and_uid));
}

static void _qq_search_before_add_with_gc_and_uid(gc_and_uid *g)
{
	GaimConnection *gc;
	guint32 uid;

	g_return_if_fail(g != NULL);

	gc = g->gc;
	uid = g->uid;
	g_return_if_fail(gc != 0 && uid != 0);

	qq_send_packet_get_info(gc, uid, TRUE);	/* we wanna see window */
	gaim_request_action
	    (gc, NULL, _("Do you wanna add this buddy?"), "", 2, g, 2,
	     _("Cancel"), NULL, _("Add"), G_CALLBACK(qq_add_buddy_with_gc_and_uid));
}

/* Send ACK if the sys message needs an ACK */
static void _qq_send_packet_ack_msg_sys(GaimConnection *gc, guint8 code, guint32 from, guint16 seq)
{
	guint8 bar, *ack, *cursor;
	gchar *str;
	gint ack_len, bytes;

	str = g_strdup_printf("%d", from);
	bar = 0x1e;
	ack_len = 1 + 1 + strlen(str) + 1 + 2;
	ack = g_newa(guint8, ack_len);
	cursor = ack;
	bytes = 0;

	bytes += create_packet_b(ack, &cursor, code);
	bytes += create_packet_b(ack, &cursor, bar);
	bytes += create_packet_data(ack, &cursor, str, strlen(str));
	bytes += create_packet_b(ack, &cursor, bar);
	bytes += create_packet_w(ack, &cursor, seq);

	g_free(str);

	if (bytes == ack_len)	/* creation OK */
		qq_send_cmd(gc, QQ_CMD_ACK_SYS_MSG, TRUE, 0, FALSE, ack, ack_len);
	else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Fail creating sys msg ACK, expect %d bytes, build %d bytes\n", ack_len, bytes);
}

/* when you are added by a person, QQ server will send sys message */
static void _qq_process_msg_sys_being_added(GaimConnection *gc, gchar *from, gchar *to, gchar *msg_utf8)
{
	gchar *message;
	GaimBuddy *b;
	guint32 uid;
	gc_and_uid *g;
	gchar *name;

	g_return_if_fail(gc != NULL && from != NULL && to != NULL);

	uid = strtol(from, NULL, 10);
	name = uid_to_gaim_name(uid);
	b = gaim_find_buddy(gc->account, name);
	g_free(name);
	if (b == NULL) {	/* the person is not in my list */
		g = g_new0(gc_and_uid, 1);
		g->gc = gc;
		g->uid = uid;	/* only need to get value */
		message = g_strdup_printf(_("You have been added by %s"), from);
		_qq_sys_msg_log_write(gc, message, from);
		gaim_request_action(gc, NULL, message,
				    _("Would like to add him?"), 2, g, 3,
				    _("Cancel"), NULL, _("Add"),
				    G_CALLBACK
				    (qq_add_buddy_with_gc_and_uid),
				    _("Search"), G_CALLBACK(_qq_search_before_add_with_gc_and_uid));
	} else {
		message = g_strdup_printf(_("%s has added you [%s]"), from, to);
		_qq_sys_msg_log_write(gc, message, from);
		gaim_notify_info(gc, NULL, message, NULL);
	}

	g_free(message);
}

/* you are rejected by the person */
static void _qq_process_msg_sys_add_contact_rejected(GaimConnection *gc, gchar *from, gchar *to, gchar *msg_utf8)
{
	gchar *message, *reason;

	g_return_if_fail(gc != NULL && from != NULL && to != NULL);

	message = g_strdup_printf(_("User %s rejected your request"), from);
	reason = g_strdup_printf(_("Reason: %s"), msg_utf8);
	_qq_sys_msg_log_write(gc, message, from);

	gaim_notify_info(gc, NULL, message, reason);
	g_free(message);
	g_free(reason);
}

/* the buddy approves your request of adding him/her as your friend */
static void _qq_process_msg_sys_add_contact_approved(GaimConnection *gc, gchar *from, gchar *to, gchar *msg_utf8)
{
	gchar *message;
	qq_data *qd;

	g_return_if_fail(gc != NULL && from != NULL && to != NULL);

	qd = (qq_data *) gc->proto_data;
	qq_add_buddy_by_recv_packet(gc, strtol(from, NULL, 10), TRUE, TRUE);

	message = g_strdup_printf(_("Use %s has approved your request"), from);
	_qq_sys_msg_log_write(gc, message, from);
	gaim_notify_info(gc, NULL, message, NULL);

	g_free(message);
}

/* someone wants to add you to his buddy list */
static void _qq_process_msg_sys_add_contact_request(GaimConnection *gc, gchar *from, gchar *to, gchar *msg_utf8)
{
	gchar *message, *reason;
	guint32 uid;
	gc_and_uid *g, *g2;
	GaimBuddy *b;
	gchar *name;

	g_return_if_fail(gc != NULL && from != NULL && to != NULL);

	uid = strtol(from, NULL, 10);
	g = g_new0(gc_and_uid, 1);
	g->gc = gc;
	g->uid = uid;

	message = g_strdup_printf(_("%s wanna add you [%s] as friends"), from, to);
	reason = g_strdup_printf(_("Message: %s"), msg_utf8);
	_qq_sys_msg_log_write(gc, message, from);

	gaim_request_action
	    (gc, NULL, message, reason, 2, g, 3,
	     _("Reject"),
	     G_CALLBACK(qq_reject_add_request_with_gc_and_uid),
	     _("Approve"),
	     G_CALLBACK(qq_approve_add_request_with_gc_and_uid),
	     _("Search"), G_CALLBACK(_qq_search_before_auth_with_gc_and_uid));

	g_free(message);
	g_free(reason);

	name = uid_to_gaim_name(uid);
	b = gaim_find_buddy(gc->account, name);
	g_free(name);
	if (b == NULL) {	/* the person is not in my list  */
		g2 = g_new0(gc_and_uid, 1);
		g2->gc = gc;
		g2->uid = strtol(from, NULL, 10);
		message = g_strdup_printf(_("%s is not in your buddy list"), from);
		gaim_request_action(gc, NULL, message,
				    _("Would you like to add him?"), 2, g2,
				    3, _("Cancel"), NULL, _("Add"),
				    G_CALLBACK
				    (qq_add_buddy_with_gc_and_uid),
				    _("Search"), G_CALLBACK(_qq_search_before_add_with_gc_and_uid));
		g_free(message);
	}
}

void qq_process_msg_sys(guint8 *buf, gint buf_len, guint16 seq, GaimConnection *gc)
{
	qq_data *qd;
	gint len;
	guint8 *data;
	gchar **segments, *code, *from, *to, *msg, *msg_utf8;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(gchar, len);

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		if (NULL == (segments = split_data(data, len, "\x1f", 4)))
			return;
		code = segments[0];
		from = segments[1];
		to = segments[2];
		msg = segments[3];

		_qq_send_packet_ack_msg_sys(gc, code[0], strtol(from, NULL, 10), seq);

		if (strtol(to, NULL, 10) != qd->uid) {	/* not to me */
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Recv sys msg to [%s], not me!, discard\n", to);
			g_strfreev(segments);
			return;
		}

		msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
		switch (strtol(code, NULL, 10)) {
		case QQ_MSG_SYS_BEING_ADDED:
			_qq_process_msg_sys_being_added(gc, from, to, msg_utf8);
			break;
		case QQ_MSG_SYS_ADD_CONTACT_REQUEST:
			_qq_process_msg_sys_add_contact_request(gc, from, to, msg_utf8);
			break;
		case QQ_MSG_SYS_ADD_CONTACT_APPROVED:
			_qq_process_msg_sys_add_contact_approved(gc, from, to, msg_utf8);
			break;
		case QQ_MSG_SYS_ADD_CONTACT_REJECTED:
			_qq_process_msg_sys_add_contact_rejected(gc, from, to, msg_utf8);
			break;
		case QQ_MSG_SYS_NEW_VERSION:
			gaim_debug(GAIM_DEBUG_WARNING, "QQ",
				   "QQ server says there is newer version than %s\n", qq_get_source_str(QQ_CLIENT));
			break;
		default:
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Recv unknown sys msg code: %s\n", code);
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "the msg is : %s\n", msg_utf8);
		}
		g_free(msg_utf8);
		g_strfreev(segments);

	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt recv msg sys\n");
	}
}
