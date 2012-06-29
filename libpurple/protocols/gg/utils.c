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
