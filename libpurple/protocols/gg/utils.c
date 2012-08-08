/**
 * @file utils.c
 *
 * purple
 *
 * Copyright (C) 2005  Bartosz Oler <bartosz@bzimage.us>
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
	int raw_len = strlen(str);
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
