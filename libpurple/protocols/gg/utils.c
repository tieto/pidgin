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

#include "utils.h"

#include "gg.h"

#include <debug.h>

uin_t ggp_str_to_uin(const char *str)
{
	char *endptr;
	uin_t uin;

	if (!str || str[0] < '0' || str[0] > '9')
		return 0;

	errno = 0;
	uin = strtoul(str, &endptr, 10);

	if (errno == ERANGE || endptr[0] != '\0')
		return 0;

	return uin;
}

const char * ggp_uin_to_str(uin_t uin)
{
	static char buff[GGP_UIN_LEN_MAX + 1];
	
	g_snprintf(buff, GGP_UIN_LEN_MAX + 1, "%u", uin);
	
	return buff;
}

static gchar * ggp_convert(const gchar *src, const char *srcenc,
	const char *dstenc)
{
	gchar *dst;
	GError *err = NULL;

	if (src == NULL)
		return NULL;

	dst = g_convert_with_fallback(src, strlen(src), dstenc, srcenc, "?",
		NULL, NULL, &err);
	if (err != NULL)
	{
		purple_debug_error("gg", "error converting from %s to %s: %s\n",
			srcenc, dstenc, err->message);
		g_error_free(err);
	}

	if (dst == NULL)
		dst = g_strdup(src);

	return dst;
}

gchar * ggp_convert_to_cp1250(const gchar *src)
{
	return ggp_convert(src, "UTF-8", "CP1250");
}

gchar * ggp_convert_from_cp1250(const gchar *src)
{
	return ggp_convert(src, "CP1250", "UTF-8");
}

gboolean ggp_password_validate(const gchar *password)
{
	const int len = strlen(password);
	if (len < 6 || len > 15)
		return FALSE;
	return g_regex_match_simple("^[ a-zA-Z0-9~`!@#$%^&*()_+=[\\]{};':\",./?"
		"<>\\\\|-]+$", password, 0, 0);
}

guint64 ggp_microtime(void)
{
	// replace with g_get_monotonic_time, when gtk 2.28 will be available
	GTimeVal time_s;
	
	g_get_current_time(&time_s);
	
	return ((guint64)time_s.tv_sec << 32) | time_s.tv_usec;
}

gchar * ggp_utf8_strndup(const gchar *str, gsize n)
{
	size_t raw_len = strlen(str);
	gchar *end_ptr;
	if (str == NULL)
		return NULL;
	if (raw_len <= n)
		return g_strdup(str);
	
	end_ptr = g_utf8_offset_to_pointer(str, g_utf8_pointer_to_offset(str, &str[n]));
	raw_len = end_ptr - str;
	
	if (raw_len > n)
	{
		end_ptr = g_utf8_prev_char(end_ptr);
		raw_len = end_ptr - str;
	}
	
	g_assert(raw_len <= n);
	
	return g_strndup(str, raw_len);
}

GSList * ggp_list_copy_to_slist_deep(GList *list, GCopyFunc func,
	gpointer user_data)
{
	GSList *new_list = NULL;
	GList *it;
	
	it = g_list_first(list);
	while (it)
	{
		new_list = g_slist_append(new_list, func(it->data, user_data));
		it = g_list_next(it);
	}
	return new_list;
}

GList * ggp_strsplit_list(const gchar *string, const gchar *delimiter,
	gint max_tokens)
{
	gchar **splitted, **it;
	GList *list = NULL;
	
	it = splitted = g_strsplit(string, delimiter, max_tokens);
	while (*it)
	{
		list = g_list_append(list, *it);
		it++;
	}
	g_free(splitted);
	
	return list;
}

gchar * ggp_strjoin_list(const gchar *separator, GList *list)
{
	gchar **str_array;
	gchar *joined;
	gint list_len, i;
	GList *it;
	
	list_len = g_list_length(list);
	str_array = g_new(gchar*, list_len + 1);
	
	it = g_list_first(list);
	i = 0;
	while (it)
	{
		str_array[i++] = it->data;
		it = g_list_next(it);
	}
	str_array[i] = NULL;
	
	joined = g_strjoinv(separator, str_array);
	g_free(str_array);
	
	return joined;
}

const gchar * ggp_ipv4_to_str(uint32_t raw_ip)
{
	static gchar buff[INET_ADDRSTRLEN];
	buff[0] = '\0';
	
	g_snprintf(buff, sizeof(buff), "%d.%d.%d.%d",
		((raw_ip >>  0) & 0xFF),
		((raw_ip >>  8) & 0xFF),
		((raw_ip >> 16) & 0xFF),
		((raw_ip >> 24) & 0xFF));
	
	return buff;
}

GList * ggp_list_truncate(GList *list, guint length, GDestroyNotify free_func)
{
	while (g_list_length(list) > length)
	{
		GList *last = g_list_last(list);
		free_func(last->data);
		list = g_list_delete_link(list, last);
	}
	return list;
}

gchar * ggp_free_if_equal(gchar *str, const gchar *pattern)
{
	if (g_strcmp0(str, pattern) == 0)
	{
		g_free(str);
		return NULL;
	}
	return str;
}

const gchar * ggp_date_strftime(const gchar *format, time_t date)
{
	GDate g_date;
	static gchar buff[30];
	
	g_date_set_time_t(&g_date, date);
	if (0 == g_date_strftime(buff, sizeof(buff), format, &g_date))
		return NULL;
	return buff;
}

time_t ggp_date_from_iso8601(const gchar *str)
{
	GTimeVal g_timeval;
	
	if (!str)
		return 0;
	if (!g_time_val_from_iso8601(str, &g_timeval))
		return 0;
	return g_timeval.tv_sec;
}
