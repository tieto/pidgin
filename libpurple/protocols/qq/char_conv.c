/**
 * @file char_conv.c
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

#include "debug.h"
#include "internal.h"

#include "char_conv.h"
#include "packet_parse.h"
#include "utils.h"

#define UTF8                  "UTF-8"
#define QQ_CHARSET_ZH_CN      "GB18030"
#define QQ_CHARSET_ENG        "ISO-8859-1"

#define QQ_NULL_MSG           "(NULL)"	/* return this if conversion fails */

/* convert a string from from_charset to to_charset, using g_convert */
/* Warning: do not return NULL */
static gchar *do_convert(const gchar *str, gssize len, const gchar *to_charset, const gchar *from_charset)
{
	GError *error = NULL;
	gchar *ret;
	gsize byte_read, byte_write;

	g_return_val_if_fail(str != NULL && to_charset != NULL && from_charset != NULL, g_strdup(QQ_NULL_MSG));

	ret = g_convert(str, len, to_charset, from_charset, &byte_read, &byte_write, &error);

	if (error == NULL) {
		return ret;	/* convert is OK */
	}

	/* convert error */
	purple_debug_error("QQ_CONVERT", "%s\n", error->message);
	qq_show_packet("Dump failed text", (guint8 *) str, (len == -1) ? strlen(str) : len);

	g_error_free(error);
	return g_strdup(QQ_NULL_MSG);
}

/*
 * take the input as a pascal string and return a converted c-string in UTF-8
 * returns the number of bytes read, return -1 if fatal error
 * the converted UTF-8 will be saved in ret
 * Return: *ret != NULL
 */
gint qq_get_vstr(gchar **ret, const gchar *from_charset, guint8 *data)
{
	guint8 len;

	g_return_val_if_fail(data != NULL && from_charset != NULL, -1);

	len = data[0];
	if (len == 0) {
		*ret = g_strdup("");
		return 1;
	}
	*ret = do_convert((gchar *) (data + 1), (gssize) len, UTF8, from_charset);

	return len + 1;
}

gint qq_put_vstr(guint8 *buf, const gchar *str_utf8, const gchar *to_charset)
{
	gchar *str;
	guint8 len;

	if (str_utf8 == NULL || (len = strlen(str_utf8)) == 0) {
		buf[0] = 0;
		return 1;
	}
	str = do_convert(str_utf8, -1, to_charset, UTF8);
	len = strlen(str_utf8);
	buf[0] = len;
	if (len > 0) {
		memcpy(buf + 1, str, len);
	}
	return 1 + len;
}

/* Warning: do not return NULL */
gchar *utf8_to_qq(const gchar *str, const gchar *to_charset)
{
	return do_convert(str, -1, to_charset, UTF8);
}

/* Warning: do not return NULL */
gchar *qq_to_utf8(const gchar *str, const gchar *from_charset)
{
	return do_convert(str, -1, UTF8, from_charset);
}

