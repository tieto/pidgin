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
#include "internal.h"		// strlen, _("get_text)
#include "debug.h"		// gaim_debug
#include "notify.h"		// gaim_notify

#include "utils.h"		// uid_to_gaim_name
#include "packet_parse.h"	// MAX_PACKET_SIZE
#include "buddy_info.h"		//
#include "char_conv.h"		// qq_to_utf8
#include "crypt.h"		// qq_crypt
#include "header_info.h"	// cmd alias
#include "infodlg.h"		// info_window
#include "keep_alive.h"		// qq_update_buddy_contact
#include "send_core.h"		// qq_send_cmd

// amount of fiedls in user info
#define QQ_CONTACT_FIELDS 				37

// There is no user id stored in the reply packet for information query
// we have to manually store the query, so that we know the query source
typedef struct _qq_info_query {
	guint32 uid;
	gboolean show_window;
	contact_info *ret_info;
} qq_info_query;

/*****************************************************************************/
// send a packet to get detailed information of uid,
// if show_window, a info window will be display upon receiving a reply
void qq_send_packet_get_info(GaimConnection * gc, guint32 uid, gboolean show_window)
{
	qq_data *qd;
	gchar *uid_str;
	GList *list;
	qq_info_query *query;
	gboolean is_exist;
	contact_info_window *info_window;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL && uid != 0);

	qd = (qq_data *) gc->proto_data;
	uid_str = g_strdup_printf("%d", uid);
	qq_send_cmd(gc, QQ_CMD_GET_USER_INFO, TRUE, 0, TRUE, uid_str, strlen(uid_str));

	if (show_window) {	// prepare the window
		is_exist = FALSE;	// see if there is already a window for this uid
		list = qd->contact_info_window;
		while (list != NULL) {
			info_window = (contact_info_window *) list->data;
			if (uid == info_window->uid) {
				is_exist = TRUE;
				break;
			} else
				list = list->next;
		}		// while list
		if (!is_exist) {	// create a new one
			info_window = g_new0(contact_info_window, 1);
			info_window->uid = uid;
			qd->contact_info_window = g_list_append(qd->contact_info_window, info_window);
		}		// if !is_exist
	}			// if show_window

	query = g_new0(qq_info_query, 1);
	query->uid = uid;
	query->show_window = show_window;
	qd->info_query = g_list_append(qd->info_query, query);

	g_free(uid_str);
}				// qq_send_packet_get_info

/*****************************************************************************/
// send packet to modify personal information, and/or change password
void qq_send_packet_modify_info(GaimConnection * gc, contact_info * info, gchar * new_passwd)
{
	GaimAccount *a;
	gchar *old_passwd, *info_field[QQ_CONTACT_FIELDS];
	gint i;
	guint8 *raw_data, *cursor, bar;

	g_return_if_fail(gc != NULL && info != NULL);

	a = gc->account;
	old_passwd = a->password;
	bar = 0x1f;
	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 128);
	cursor = raw_data;

	g_memmove(info_field, info, sizeof(gchar *) * QQ_CONTACT_FIELDS);

	if (new_passwd == NULL || strlen(new_passwd) == 0)
		create_packet_b(raw_data, &cursor, bar);
	else {			// we gonna change passwd
		create_packet_data(raw_data, &cursor, old_passwd, strlen(old_passwd));
		create_packet_b(raw_data, &cursor, bar);
		create_packet_data(raw_data, &cursor, new_passwd, strlen(new_passwd));
	}

	// important!, skip the first uid entry
	for (i = 1; i < QQ_CONTACT_FIELDS; i++) {
		create_packet_b(raw_data, &cursor, bar);
		create_packet_data(raw_data, &cursor, info_field[i], strlen(info_field[i]));
	}
	create_packet_b(raw_data, &cursor, bar);

	qq_send_cmd(gc, QQ_CMD_UPDATE_INFO, TRUE, 0, TRUE, raw_data, cursor - raw_data);

}				// qq_send_packet_modify_info

/*****************************************************************************/
// process the reply of modidy_info packet
void qq_process_modify_info_reply(guint8 * buf, gint buf_len, GaimConnection * gc)
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
		if (qd->uid == atoi(data)) {	// return should be my uid
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "Update info ACK OK\n");
			gaim_notify_info(gc, NULL, _("You information have been updated"), NULL);
		}
	} else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt modify info reply\n");

}				// qq_process_modify_info_reply

/*****************************************************************************/
// after getting info or modify myself, refresh the buddy list accordingly
void qq_refresh_buddy_and_myself(contact_info * info, GaimConnection * gc)
{
	GaimBuddy *b;
	qq_data *qd;
	qq_buddy *q_bud;
	gchar *alias_utf8;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	alias_utf8 = qq_to_utf8(info->nick, QQ_CHARSET_DEFAULT);
	if (qd->uid == strtol(info->uid, NULL, 10)) {	// it is me
		qd->my_icon = strtol(info->face, NULL, 10);
		if (alias_utf8 != NULL)
			gaim_account_set_alias(gc->account, alias_utf8);
	}
	// update buddy list (including myself, if myself is the buddy)
	b = gaim_find_buddy(gc->account, uid_to_gaim_name(strtol(info->uid, NULL, 10)));
	q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;
	if (q_bud != NULL) {	// I have this buddy
		q_bud->age = strtol(info->age, NULL, 10);
		q_bud->gender = strtol(info->gender, NULL, 10);
		q_bud->icon = strtol(info->face, NULL, 10);
		if (alias_utf8 != NULL)
			q_bud->nickname = g_strdup(alias_utf8);
		qq_update_buddy_contact(gc, q_bud);
	}			// if q_bud
	g_free(alias_utf8);
}				// qq_refresh_buddy_and_myself

/*****************************************************************************/
// process reply to get_info packet
void qq_process_get_info_reply(guint8 * buf, gint buf_len, GaimConnection * gc)
{
	gint len;
	guint8 *data;
	gchar **segments;
	qq_info_query *query;
	qq_data *qd;
	contact_info *info;
	contact_info_window *info_window;
	gboolean show_window;
	GList *list, *query_list;

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
		qq_refresh_buddy_and_myself(info, gc);

		query_list = qd->info_query;
		show_window = FALSE;
		while (query_list != NULL) {
			query = (qq_info_query *) query_list->data;
			if (query->uid == atoi(info->uid)) {
				show_window = query->show_window;
				qd->info_query = g_list_remove(qd->info_query, qd->info_query->data);
				g_free(query);
				break;
			}
			query_list = query_list->next;
		}		// while query_list

		if (!show_window) {
			g_strfreev(segments);
			return;
		}
		// if not show_window, we can not find the window here either
		list = qd->contact_info_window;
		while (list != NULL) {
			info_window = (contact_info_window *) (list->data);
			if (info_window->uid == atoi(info->uid)) {
				if (info_window->window)
					qq_refresh_contact_info_dialog(info, gc, info_window);
				else
					qq_show_contact_info_dialog(info, gc, info_window);
				break;
			} else
				list = list->next;
		}		// while list
		g_strfreev(segments);
	} else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt get info reply\n");

}				// qq_process_get_info_reply

/*****************************************************************************/
void qq_info_query_free(qq_data * qd)
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
}				// qq_add_buddy_request_free

/*****************************************************************************/
// END OF FILE
