/**
 * @file buddy_opt.h
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

#ifndef _QQ_BUDDY_OPT_H_
#define _QQ_BUDDY_OPT_H_

#include <glib.h>
#include "connection.h"

#include "qq.h"

enum {
	QQ_AUTH_INFO_BUDDY = 0x01,
	QQ_AUTH_INFO_ROOM = 0x02,

	QQ_AUTH_INFO_ADD_BUDDY = 0x0001,
	QQ_AUTH_INFO_TEMP_SESSION = 0x0003,
	QQ_AUTH_INFO_CLUSTER = 0x0002,
	QQ_AUTH_INFO_REMOVE_BUDDY = 0x0006,
	QQ_AUTH_INFO_UPDATE_BUDDY_INFO = 0x0007
};

enum {
	QQ_QUESTION_GET = 0x01,
	QQ_QUESTION_SET = 0x02,
	QQ_QUESTION_REQUEST = 0x03,		/* get question only*/
	QQ_QUESTION_ANSWER = 0x04
};

void qq_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
void qq_change_buddys_group(PurpleConnection *gc, const char *who,
		const char *old_group, const char *new_group);
void qq_remove_buddy_and_me(PurpleBlistNode * node);
void qq_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);

void qq_process_remove_buddy(PurpleConnection *gc, guint8 *data, gint data_len, UID uid);
void qq_process_buddy_remove_me(PurpleConnection *gc, guint8 *data, gint data_len, UID uid);
void qq_process_add_buddy_no_auth(PurpleConnection *gc,
		guint8 *data, gint data_len, UID uid);
void qq_process_add_buddy_no_auth_ex(PurpleConnection *gc,
		guint8 *data, gint data_len, UID uid);
void qq_process_add_buddy_auth(guint8 *data, gint data_len, PurpleConnection *gc);
void qq_process_buddy_from_server(PurpleConnection *gc, int funct,
		gchar *from, gchar *to, guint8 *data, gint data_len);

void qq_process_buddy_check_code(PurpleConnection *gc, guint8 *data, gint data_len);

void qq_request_auth_code(PurpleConnection *gc, guint8 cmd, guint16 sub_cmd, UID uid);
void qq_process_auth_code(PurpleConnection *gc, guint8 *data, gint data_len, UID uid);
void qq_request_question(PurpleConnection *gc,
		guint8 cmd, UID uid, const gchar *question_utf8, const gchar *answer_utf8);
void qq_process_question(PurpleConnection *gc, guint8 *data, gint data_len, UID uid);

void qq_process_add_buddy_auth_ex(PurpleConnection *gc, guint8 *data, gint data_len, guint32 ship32);

qq_buddy_data *qq_buddy_data_find(PurpleConnection *gc, UID uid);
void qq_buddy_data_free(qq_buddy_data *bd);

PurpleBuddy *qq_buddy_new(PurpleConnection *gc, UID uid);
PurpleBuddy *qq_buddy_find_or_new(PurpleConnection *gc, UID uid);
PurpleBuddy *qq_buddy_find(PurpleConnection *gc, UID uid);
PurpleGroup *qq_group_find_or_new(const gchar *group_name);
#endif
