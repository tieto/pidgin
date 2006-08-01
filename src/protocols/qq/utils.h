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

#ifndef _QQ_MY_UTILS_H_
#define _QQ_MY_UTILS_H_

#include <stdio.h>
#include <glib.h>

#define QQ_NAME_PREFIX    "qq-"

gchar *get_name_by_index_str(gchar **array, const gchar *index_str, gint amount);
gchar *get_index_str_by_name(gchar **array, const gchar *name, gint amount);
gint qq_string_to_dec_value(const gchar *str);

gchar **split_data(guint8 *data, gint len, const gchar *delimit, gint expected_fields);
gchar *gen_ip_str(guint8 *ip);
guint8 *str_ip_gen(gchar *str);
gchar *uid_to_gaim_name(guint32 uid);

guint32 gaim_name_to_uid(const gchar *name);

gchar *get_icon_name(gint set, gint suffix);

void try_dump_as_gbk(guint8 *data, gint len);

guint8 *hex_str_to_bytes(const gchar *buf);
gchar *hex_dump_to_str(const guint8 *buf, gint buf_len);

#endif
