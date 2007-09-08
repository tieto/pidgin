/**
 * @file utils.h
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

#ifndef _QQ_MY_UTILS_H_
#define _QQ_MY_UTILS_H_

#include <stdio.h>
#include <glib.h>

gchar *get_name_by_index_str(gchar **array, const gchar *index_str, gint amount);
gchar *get_index_str_by_name(gchar **array, const gchar *name, gint amount);
gint qq_string_to_dec_value(const gchar *str);

gchar **split_data(guint8 *data, gint len, const gchar *delimit, gint expected_fields);
guint8 *_gen_session_md5(gint uid, guint8 *session_key);

gchar *gen_ip_str(guint8 *ip);
guint8 *str_ip_gen(gchar *str);

guint32 purple_name_to_uid(const gchar *name);
gchar *uid_to_purple_name(guint32 uid);
gchar *chat_name_to_purple_name(const gchar *const name);

gchar *face_to_icon_str(gint face);

void try_dump_as_gbk(const guint8 *const data, gint len);

guint8 *hex_str_to_bytes(const gchar *buf, gint *out_len);
gchar *hex_dump_to_str(const guint8 *buf, gint buf_len);

const gchar *qq_buddy_icon_dir(void);
const gchar *qq_win32_buddy_icon_dir(void);

#endif
