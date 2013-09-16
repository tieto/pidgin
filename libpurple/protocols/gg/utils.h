/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#ifndef _GGP_UTILS_H
#define _GGP_UTILS_H

#include <internal.h>
#include <libgadu.h>
#include <json-glib/json-glib.h>

/**
 * Converts stringified UIN to uin_t.
 *
 * @param str The string to convert.
 *
 * @return Converted UIN or 0 if an error occurred.
 */
uin_t ggp_str_to_uin(const char *str);

/**
 * Stringifies UIN.
 *
 * @param uin UIN to stringify.
 *
 * @return Stringified UIN.
 */
const char * ggp_uin_to_str(uin_t uin);

/**
 * Gets UIN for the account.
 *
 * @param gc The connection, in which account is connected.
 * @return UIN for this account.
 */
uin_t ggp_get_my_uin(PurpleConnection *gc);

/**
 * Converts encoding of a given string from UTF-8 to CP1250.
 *
 * @param src Input string.
 *
 * @return Converted string (must be freed with g_free). If src is NULL,
 * then NULL is returned.
 */
gchar * ggp_convert_to_cp1250(const gchar *src);

/**
 * Converts encoding of a given string from CP1250 to UTF-8.
 *
 * @param src Input string.
 *
 * @return Converted string (must be freed with g_free). If src is NULL,
 * then NULL is returned.
 */
gchar * ggp_convert_from_cp1250(const gchar *src);

gboolean ggp_password_validate(const gchar *password);

gchar * ggp_utf8_strndup(const gchar *str, gsize n);

GSList * ggp_list_copy_to_slist_deep(GList *list, GCopyFunc func,
	gpointer user_data);

GList * ggp_strsplit_list(const gchar *string, const gchar *delimiter,
	gint max_tokens);

gchar * ggp_strjoin_list(const gchar *separator, GList *list);

GList * ggp_list_truncate(GList *list, guint length, GDestroyNotify free_func);

gchar * ggp_free_if_equal(gchar *str, const gchar *pattern);

const gchar * ggp_date_strftime(const gchar *format, time_t date);

time_t ggp_date_from_iso8601(const gchar *str);

uint64_t * ggp_uint64dup(uint64_t val);

gint ggp_int64_compare(gconstpointer a, gconstpointer b);

JsonParser * ggp_json_parse(const gchar *data);

#endif /* _GGP_UTILS_H */
