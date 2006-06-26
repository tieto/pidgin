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

// START OF FILE
/*****************************************************************************/
#include "debug.h"		// gaim_debug
#include "internal.h"		// strlen, _("get_text")
#include "notify.h"		// gaim_notify
#include "request.h"		// gaim_request_input

#include "utils.h"		// uid_to_gaim_name, gaim_name_to_uid
#include "packet_parse.h"	// create_packet, read_packet
#include "buddy_info.h"		// qq_send_packet_get_info
#include "buddy_list.h"		// qq_send_packet_get_buddies_online
#include "buddy_opt.h"
#include "char_conv.h"		// qq_to_utf8
#include "crypt.h"		// qq_crypt
#include "header_info.h"	// cmd alias
#include "keep_alive.h"		// qq_update_buddy_contact
#include "im.h"			// QQ_MSG_IM_MAX
#include "send_core.h"		// qq_send_cmd

#define GAIM_GROUP_QQ_FORMAT          "QQ (%s)"
#define GAIM_GROUP_QQ_UNKNOWN         "QQ Unknown"
#define GAIM_GROUP_QQ_BLOCKED         "QQ Blocked"

#define QQ_REMOVE_BUDDY_REPLY_OK      0x00
#define QQ_REMOVE_SELF_REPLY_OK       0x00
#define QQ_ADD_BUDDY_AUTH_REPLY_OK    0x30	// ASCii value of "0"

enum {
	QQ_MY_AUTH_APPROVE = 0x30,	// ASCii value of "0"
	QQ_MY_AUTH_REJECT = 0x31,	// ASCii value of "1"
	QQ_MY_AUTH_REQUEST = 0x32,	// ASCii value of "2"
};

typedef struct _qq_add_buddy_request {
	guint32 uid;
	guint16 seq;
} qq_add_buddy_request;

/*****************************************************************************/
// send packet to remove a buddy from my buddy list
static void _qq_send_packet_remove_buddy(GaimConnection * gc, guint32 uid)
{
	gchar *uid_str;

	g_return_if_fail(gc != NULL && uid > 0);

	uid_str = g_strdup_printf("%d", uid);
	qq_send_cmd(gc, QQ_CMD_DEL_FRIEND, TRUE, 0, TRUE, uid_str, strlen(uid_str));

	g_free(uid_str);
}				// _qq_send_packet_remove_buddy

/*****************************************************************************/
// try to remove myself from someone's buddy list
static void _qq_send_packet_remove_self_from(GaimConnection * gc, guint32 uid)
{
	guint8 *raw_data, *cursor;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL && uid > 0);

	raw_data = g_newa(guint8, 4);
	cursor = raw_data;
	create_packet_dw(raw_data, &cursor, uid);

	qq_send_cmd(gc, QQ_CMD_REMOVE_SELF, TRUE, 0, TRUE, raw_data, 4);

}				// _qq_send_packet_add_buddy

/*****************************************************************************/
// try to add a buddy without authentication
static void _qq_send_packet_add_buddy(GaimConnection * gc, guint32 uid)
{
	qq_data *qd;
	qq_add_buddy_request *req;
	gchar *uid_str;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL && uid > 0);

	// we need to send the ascii code of this uid to qq server
	uid_str = g_strdup_printf("%d", uid);
	qq_send_cmd(gc, QQ_CMD_ADD_FRIEND_WO_AUTH, TRUE, 0, TRUE, uid_str, strlen(uid_str));
	g_free(uid_str);

	// must be set after sending packet to get the correct send_seq
	qd = (qq_data *) gc->proto_data;
	req = g_new0(qq_add_buddy_request, 1);
	req->seq = qd->send_seq;
	req->uid = uid;
	qd->add_buddy_request = g_list_append(qd->add_buddy_request, req);
}				// _qq_send_packet_add_buddy

/*****************************************************************************/
// this buddy needs authentication, text conversion is done at lowest level
static void _qq_send_packet_buddy_auth(GaimConnection * gc, guint32 uid, const gchar response, const gchar * text)
{
	gchar *text_qq, *uid_str;
	guint8 bar, *cursor, *raw_data;

	g_return_if_fail(gc != NULL && uid != 0);

	uid_str = g_strdup_printf("%d", uid);
	bar = 0x1f;
	raw_data = g_newa(guint8, QQ_MSG_IM_MAX);
	cursor = raw_data;

	create_packet_data(raw_data, &cursor, uid_str, strlen(uid_str));
	create_packet_b(raw_data, &cursor, bar);
	create_packet_b(raw_data, &cursor, response);

	if (text != NULL) {
		text_qq = utf8_to_qq(text, QQ_CHARSET_DEFAULT);
		create_packet_b(raw_data, &cursor, bar);
		create_packet_data(raw_data, &cursor, text_qq, strlen(text_qq));
		g_free(text_qq);
	}

	qq_send_cmd(gc, QQ_CMD_BUDDY_AUTH, TRUE, 0, TRUE, raw_data, cursor - raw_data);
	g_free(uid_str);
}				// _qq_send_packet_buddy_auth


/*****************************************************************************/
static void _qq_send_packet_add_buddy_auth_with_gc_and_uid(gc_and_uid * g, const gchar * text) {
	GaimConnection *gc;
	guint32 uid;
	g_return_if_fail(g != NULL);

	gc = g->gc;
	uid = g->uid;
	g_return_if_fail(gc != NULL && uid != 0);

	_qq_send_packet_buddy_auth(gc, uid, QQ_MY_AUTH_REQUEST, text);
	g_free(g);
}				// qq_send_packet_add_buddy_auth

/*****************************************************************************/
// the real packet to reject and request is sent from here
static void _qq_reject_add_request_real(gc_and_uid * g, const gchar * reason)
{
	gint uid;
	GaimConnection *gc;

	g_return_if_fail(g != NULL);

	gc = g->gc;
	uid = g->uid;
	g_return_if_fail(gc != NULL && uid != 0);

	_qq_send_packet_buddy_auth(gc, uid, QQ_MY_AUTH_REJECT, reason);
	g_free(g);
}				// _qq_reject_add_request_real

/*****************************************************************************/
// we approve other's request of adding me as friend
void qq_approve_add_request_with_gc_and_uid(gc_and_uid * g)
{
	gint uid;
	GaimConnection *gc;

	g_return_if_fail(g != NULL);

	gc = g->gc;
	uid = g->uid;
	g_return_if_fail(gc != NULL && uid != 0);

	_qq_send_packet_buddy_auth(gc, uid, QQ_MY_AUTH_APPROVE, NULL);
	g_free(g);
}				// qq_approve_add_request_with_gc_and_uid

/*****************************************************************************/
void qq_do_nothing_with_gc_and_uid(gc_and_uid * g, const gchar * msg)
{
	g_free(g);
}				// qq_do_nothing_with_gc_and_uid

/*****************************************************************************/
// we reject other's request of adding me as friend
void qq_reject_add_request_with_gc_and_uid(gc_and_uid * g)
{
	gint uid;
	gchar *msg1, *msg2;
	GaimConnection *gc;
	gc_and_uid *g2;

	g_return_if_fail(g != NULL);

	gc = g->gc;
	uid = g->uid;
	g_return_if_fail(gc != NULL && uid != 0);

	g_free(g);

	g2 = g_new0(gc_and_uid, 1);
	g2->gc = gc;
	g2->uid = uid;

	msg1 = g_strdup_printf(_("You rejected %d's request"), uid);
	msg2 = g_strdup(_("Input your reason:"));

	gaim_request_input(gc, _("Reject request"), msg1, msg2,
			   _("Sorry, you are not my type..."), TRUE, FALSE,
			   NULL, _("Reject"), G_CALLBACK(_qq_reject_add_request_real), _("Cancel"), NULL, g2);

}				// qq_reject_add_request_with_gc_and_uid

/*****************************************************************************/
void qq_add_buddy_with_gc_and_uid(gc_and_uid * g)
{
	gint uid;
	GaimConnection *gc;

	g_return_if_fail(g != NULL);

	gc = g->gc;
	uid = g->uid;
	g_return_if_fail(gc != NULL && uid != 0);

	_qq_send_packet_add_buddy(gc, uid);
	g_free(g);
}				// qq_add_buddy_with_gc_and_uid

/*****************************************************************************/
void qq_block_buddy_with_gc_and_uid(gc_and_uid * g)
{
	guint32 uid;
	GaimConnection *gc;
	GaimBuddy buddy;
	GaimGroup group;

	g_return_if_fail(g != NULL);

	gc = g->gc;
	uid = g->uid;
	g_return_if_fail(gc != NULL && uid > 0);

	buddy.name = uid_to_gaim_name(uid);
	group.name = GAIM_GROUP_QQ_BLOCKED;

	qq_remove_buddy(gc, &buddy, &group);
	_qq_send_packet_remove_self_from(gc, uid);

}				// qq_block_buddy_with_gc_and_uid

/*****************************************************************************/
//  process reply to add_buddy_auth request
void qq_process_add_buddy_auth_reply(guint8 * buf, gint buf_len, GaimConnection * gc) {
	qq_data *qd;
	gint len;
	guint8 *data, *cursor, reply;
	gchar **segments, *msg_utf8;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);
	cursor = data;

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		read_packet_b(data, &cursor, len, &reply);
		if (reply != QQ_ADD_BUDDY_AUTH_REPLY_OK) {
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Add buddy with auth request fails\n");
			if (NULL == (segments = split_data(data, len, "\x1f", 2)))
				return;
			msg_utf8 = qq_to_utf8(segments[1], QQ_CHARSET_DEFAULT);
			gaim_notify_error(gc, NULL, _("Add buddy with auth request fails"), msg_utf8);
			g_free(msg_utf8);
		} else
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "Add buddy with auth request OK\n");
	} else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt add buddy with auth reply\n");

}				// qq_process_add_buddy_auth_reply

/*****************************************************************************/
// process the server reply for my request to remove a buddy
void qq_process_remove_buddy_reply(guint8 * buf, gint buf_len, GaimConnection * gc) {
	qq_data *qd;
	gint len;
	guint8 *data, *cursor, reply;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		cursor = data;
		read_packet_b(data, &cursor, len, &reply);
		if (reply != QQ_REMOVE_BUDDY_REPLY_OK)
			// there is no reason return from server
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Remove buddy fails\n");
		else {		// if reply
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "Remove buddy OK\n");
			gaim_notify_info(gc, NULL, _("You have successfully removed a buddy"), NULL);
		}
	} else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt remove buddy reply\n");

}				// qq_process_remove_buddy_reply


/*****************************************************************************/
// process the server reply for my request to remove myself from a buddy
void qq_process_remove_self_reply(guint8 * buf, gint buf_len, GaimConnection * gc) {
	qq_data *qd;
	gint len;
	guint8 *data, *cursor, reply;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		cursor = data;
		read_packet_b(data, &cursor, len, &reply);
		if (reply != QQ_REMOVE_SELF_REPLY_OK)
			// there is no reason return from server
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Remove self fails\n");
		else {		// if reply
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "Remove self from a buddy OK\n");
			gaim_notify_info(gc, NULL, _("You have successfully removed yourself from a buddy"), NULL);
		}
	} else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt remove self reply\n");

}				// qq_process_remove_buddy_reply

/*****************************************************************************/
void qq_process_add_buddy_reply(guint8 * buf, gint buf_len, guint16 seq, GaimConnection * gc) {
	qq_data *qd;
	gint len, for_uid;
	gchar *msg, *data, **segments, *uid, *reply;
	GList *list;
	GaimBuddy *b;
	gc_and_uid *g;
	qq_add_buddy_request *req;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(buf != NULL && buf_len != 0);

	for_uid = 0;
	qd = (qq_data *) gc->proto_data;
	len = buf_len;

	list = qd->add_buddy_request;
	while (list != NULL) {
		req = (qq_add_buddy_request *) list->data;
		if (req->seq == seq) {	// reply to this
			for_uid = req->uid;
			qd->add_buddy_request = g_list_remove(qd->add_buddy_request, qd->add_buddy_request->data);
			g_free(req);
			break;
		}
		list = list->next;
	}			// while list

	if (for_uid == 0) {	// we have no record for this
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "We have no record for add buddy reply [%d], discard\n", seq);
		return;
	} else
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "Add buddy reply [%d] is for id [%d]\n", seq, for_uid);

	data = g_newa(guint8, len);

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		if (NULL == (segments = split_data(data, len, "\x1f", 2)))
			return;
		uid = segments[0];
		reply = segments[1];
		if (strtol(uid, NULL, 10) != qd->uid) {	// should not happen
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Add buddy reply is to [%s], not me!", uid);
			g_strfreev(segments);
			return;
		}		// if uid

		if (strtol(reply, NULL, 10) > 0) {	// need auth
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Add buddy attempt fails, need authentication\n");
			b = gaim_find_buddy(gc->account, uid_to_gaim_name(for_uid));
			if (b != NULL)
				gaim_blist_remove_buddy(b);
			g = g_new0(gc_and_uid, 1);
			g->gc = gc;
			g->uid = for_uid;
			msg = g_strdup_printf(_("User %d needs authentication"), for_uid);
			gaim_request_input(gc, NULL, msg,
					   _("Input request here"),
					   _("Would you be my friend?"),
					   TRUE, FALSE, NULL, _("Send"),
					   G_CALLBACK
					   (_qq_send_packet_add_buddy_auth_with_gc_and_uid),
					   _("Cancel"), G_CALLBACK(qq_do_nothing_with_gc_and_uid), g);
			g_free(msg);
		} else {	// add OK
			qq_add_buddy_by_recv_packet(gc, for_uid, TRUE, TRUE);
			msg = g_strdup_printf(_("You have added %d in buddy list"), for_uid);
			gaim_notify_info(gc, NULL, msg, NULL);
			g_free(msg);
		}		// if reply
		g_strfreev(segments);
	} else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt add buddy reply\n");

}				// qq_process_add_buddy_reply

/*****************************************************************************/
GaimGroup *qq_get_gaim_group(const gchar * group_name)
{
	GaimGroup *g;

	g_return_val_if_fail(group_name != NULL, NULL);

	g = gaim_find_group(group_name);
	if (g == NULL) {
		g = gaim_group_new(group_name);
		gaim_blist_add_group(g, NULL);
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Add new group: %s\n", group_name);
	}			// if (g == NULL) 

	return g;
}				// qq_get_gaim_group

/*****************************************************************************/
// we add new buddy, if the received packet is from someone not in my list
// return the GaimBuddy that is just created
GaimBuddy *qq_add_buddy_by_recv_packet(GaimConnection * gc, guint32 uid, gboolean is_known, gboolean create) {
	GaimAccount *a;
	GaimBuddy *b;
	GaimGroup *g;
	qq_data *qd;
	qq_buddy *q_bud;
	gchar *name, *group_name;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, NULL);

	a = gc->account;
	qd = (qq_data *) gc->proto_data;
	g_return_val_if_fail(a != NULL && uid != 0, NULL);

	group_name = is_known ?
	    g_strdup_printf(GAIM_GROUP_QQ_FORMAT, gaim_account_get_username(a)) : g_strdup(GAIM_GROUP_QQ_UNKNOWN);

	g = qq_get_gaim_group(group_name);

	name = uid_to_gaim_name(uid);
	b = gaim_find_buddy(gc->account, name);
	// remove old, we can not simply return here
	// because there might be old local copy of this buddy
	if (b != NULL)
		gaim_blist_remove_buddy(b);

	b = gaim_buddy_new(a, name, NULL);

	if (!create)
		b->proto_data = NULL;
	else {
		q_bud = g_new0(qq_buddy, 1);
		q_bud->uid = uid;
		b->proto_data = q_bud;
		qd->buddies = g_list_append(qd->buddies, q_bud);
		qq_send_packet_get_info(gc, q_bud->uid, FALSE);
		qq_send_packet_get_buddies_online(gc, QQ_FRIENDS_ONLINE_POSITION_START);
	}			// if !create

	gaim_blist_add_buddy(b, NULL, g, NULL);
	gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Add new buddy: [%s]\n", name);

	g_free(name);
	g_free(group_name);

	return b;
}				// qq_add_buddy_by_recv_packet

/*****************************************************************************/
// add a buddy and send packet to QQ server
// note that when gaim load local cached buddy list into its blist
// it also calls this funtion, so we have to 
// define qd->logged_in=TRUE AFTER serv_finish_login(gc)
void qq_add_buddy(GaimConnection * gc, GaimBuddy * buddy, GaimGroup * group) {
	qq_data *qd;
	guint32 uid;
	GaimBuddy *b;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	qd = (qq_data *) gc->proto_data;
	if (!qd->logged_in)
		return;		// IMPORTANT !

	uid = gaim_name_to_uid(buddy->name);
	if (uid > 0)
		_qq_send_packet_add_buddy(gc, uid);
	else {
		b = gaim_find_buddy(gc->account, buddy->name);
		if (b != NULL)
			gaim_blist_remove_buddy(b);
		gaim_notify_error(gc, NULL,
				  _("QQid Error"),
				  _("Invalid QQid, to add buddy 1234567, \nyou should input qq-1234567"));
	}
}				// _qq_add_buddy

/*****************************************************************************/
// remove a buddy and send packet to QQ server accordingly
void qq_remove_buddy(GaimConnection * gc, GaimBuddy * buddy, GaimGroup * group) {
	qq_data *qd;
	GaimBuddy *b;
	qq_buddy *q_bud;
	guint32 uid;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	qd = (qq_data *) gc->proto_data;
	uid = gaim_name_to_uid(buddy->name);

	if (!qd->logged_in)
		return;

	if (uid > 0)
		_qq_send_packet_remove_buddy(gc, uid);

	b = gaim_find_buddy(gc->account, buddy->name);
	if (b != NULL) {
		q_bud = (qq_buddy *) b->proto_data;
		if (q_bud != NULL)
			qd->buddies = g_list_remove(qd->buddies, q_bud);
		else
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "We have no qq_buddy record for %s\n", buddy->name);
		// remove buddy on blist, this does not trigger qq_remove_buddy again
		// do this only if the request comes from block request,
		// otherwise gaim segmentation fault
		if (g_ascii_strcasecmp(group->name, GAIM_GROUP_QQ_BLOCKED)
		    == 0)
			gaim_blist_remove_buddy(b);
	}			// if b != NULL
}				// _qq_remove_buddy

/*****************************************************************************/
// free add buddy request queue
void qq_add_buddy_request_free(qq_data * qd)
{
	gint i;
	qq_add_buddy_request *p;

	g_return_if_fail(qd != NULL);

	i = 0;
	while (qd->add_buddy_request) {
		p = (qq_add_buddy_request *) (qd->add_buddy_request->data);
		qd->add_buddy_request = g_list_remove(qd->add_buddy_request, p);
		g_free(p);
		i++;
	}
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "%d add buddy requests are freed!\n", i);
}				// qq_add_buddy_request_free

/*****************************************************************************/
// free up all qq_buddy
void qq_buddies_list_free(GaimAccount *account, qq_data * qd)
{
	gint i;
	qq_buddy *p;
	gchar *name;
	GaimBuddy *b;

	g_return_if_fail(qd != NULL);

	i = 0;
	while (qd->buddies) {
		p = (qq_buddy *) (qd->buddies->data);
		qd->buddies = g_list_remove(qd->buddies, p);
		// added by gfhuang, for relogin crash bug
		name = uid_to_gaim_name(p->uid);
		b = gaim_find_buddy(account, name);   	
		if(b != NULL) b->proto_data = NULL;
		else {
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "qq_buddy %s not found in gaim proto_data\n", name);
		}
		g_free(name);

		g_free(p);
		i++;
	}
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "%d qq_buddy structures are freed!\n", i);

}				// qq_buddies_list_free

/*****************************************************************************/
// END OF FILE
