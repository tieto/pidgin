/**
 * @file char_conv.c
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "debug.h"
#include "internal.h"

#include "char_conv.h"
#include "packet_parse.h"
#include "qq.h"
#include "utils.h"

#define QQ_SMILEY_AMOUNT      96

#define UTF8                  "UTF-8"
#define QQ_CHARSET_ZH_CN      "GBK"
#define QQ_CHARSET_ENG        "ISO-8859-1"

#define QQ_NULL_MSG           "(NULL)"	/* return this if conversion fails */
#define QQ_NULL_SMILEY        "(SM)"	/* return this if smiley conversion fails */

/* a debug function */
void _qq_show_packet(const gchar *desc, const guint8 *buf, gint len);

const gchar qq_smiley_map[QQ_SMILEY_AMOUNT] = {
	0x41, 0x43, 0x42, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x73,
	0x74, 0x75, 0x76, 0x77, 0x8a, 0x8b, 0x8c, 0x8d,
	0x8e, 0x8f, 0x78, 0x79, 0x7a, 0x7b, 0x90, 0x91,
	0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x59, 0x5a, 0x5c, 0x58, 0x57, 0x55, 0x7c, 0x7d,
	0x7e, 0x7f, 0x9a, 0x9b, 0x60, 0x67, 0x9c, 0x9d,
	0x9e, 0x5e, 0x9f, 0x89, 0x80, 0x81, 0x82, 0x62,
	0x63, 0x64, 0x65, 0x66, 0x83, 0x68, 0x84, 0x85,
	0x86, 0x87, 0x6b, 0x6e, 0x6f, 0x70, 0x88, 0xa0,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x56, 0x5b, 0x5d,
	0x5f, 0x61, 0x69, 0x6a, 0x6c, 0x6d, 0x71, 0x72
};


const gchar *gaim_smiley_map[QQ_SMILEY_AMOUNT] = {
	"/jy", "/pz", "/se", "/fd", "/dy", "/ll", "/hx", "/bz",
	"/shui", "/dk	", "/gg", "/fn", "/tp", "/cy", "/wx", "/ng",
	"/kuk", "/feid", "/zk", "/tu", "/tx", "/ka", "/by", "/am",
	"/jie", "/kun", "/jk", "/lh", "/hanx", "/db", "/fendou",
	"/zhm",
	"/yiw", "/xu", "/yun", "/zhem", "/shuai", "/kl", "/qiao",
	"/zj",
	"/shan", "/fad", "/aiq", "/tiao", "/zhao", "/mm", "/zt",
	"/maom",
	"/xg", "/yb", "/qianc", "/dp", "/bei", "/dg", "/shd",
	"/zhd",
	"/dao", "/zq", "/yy", "/bb", "/gf", "/fan", "/yw", "/mg",
	"/dx", "/wen", "/xin", "/xs", "/hy", "/lw", "/dh", "/sj",
	"/yj", "/ds", "/ty", "/yl", "/qiang", "/ruo", "/ws",
	"/shl",
	"/dd", "/mn", "/hl", "/mamao", "/qz", "/fw", "/oh", "/bj",
	"/qsh", "/xig", "/xy", "/duoy", "/xr", "/xixing", "/nv",
	"/nan"
};

/* these functions parse font-attr */
static gchar _get_size(gchar font_attr)
{
	return font_attr & 0x1f;
}

static gboolean _check_bold(gchar font_attr)
{
	return (font_attr & 0x20) ? TRUE : FALSE;
}

static gboolean _check_italic(gchar font_attr)
{
	return (font_attr & 0x40) ? TRUE : FALSE;
}

static gboolean _check_underline(gchar font_attr)
{
	return (font_attr & 0x80) ? TRUE : FALSE;
}

/* convert a string from from_charset to to_charset, using g_convert */
static gchar *_my_convert(const gchar *str, gssize len, const gchar *to_charset, const gchar *from_charset) 
{
	GError *error = NULL;
	gchar *ret;
	gsize byte_read, byte_write;

	g_return_val_if_fail(str != NULL && to_charset != NULL && from_charset != NULL, g_strdup(QQ_NULL_MSG));

	ret = g_convert(str, len, to_charset, from_charset, &byte_read, &byte_write, &error);

	if (error == NULL)
		return ret;	/* conversion is OK */
	else {			/* conversion error */
		gchar *failed = hex_dump_to_str((guint8 *) str, (len == -1) ? strlen(str) : len);
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "%s\n", error->message);
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Dump failed text\n%s", failed);
		g_free(failed);
		g_error_free(error);
		return g_strdup(QQ_NULL_MSG);
	}
}

/* take the input as a pascal string and return a converted c-string in UTF-8
 * returns the number of bytes read, return -1 if fatal error
 * the converted UTF-8 will be saved in ret */
gint convert_as_pascal_string(guint8 *data, gchar **ret, const gchar *from_charset) 
{
	guint8 len;

	g_return_val_if_fail(data != NULL && from_charset != NULL, -1);

	len = data[0];
	*ret = _my_convert((gchar *) (data + 1), (gssize) len, UTF8, from_charset);

	return len + 1;
}

/* convert QQ formatted msg to Gaim formatted msg (and UTF-8) */
gchar *qq_encode_to_gaim(guint8 *data, gint len, const gchar *msg)
{
	GString *encoded;
	guint8 font_attr, font_size, color[3], bar, *cursor;
	gboolean is_bold, is_italic, is_underline;
	guint16 charset_code;
	gchar *font_name, *color_code, *msg_utf8, *tmp, *ret;

	cursor = data;
	_qq_show_packet("QQ_MESG recv for font style", data, len);

	read_packet_b(data, &cursor, len, &font_attr);
	read_packet_data(data, &cursor, len, color, 3);	/* red,green,blue */
	color_code = g_strdup_printf("#%02x%02x%02x", color[0], color[1], color[2]);

	read_packet_b(data, &cursor, len, &bar);	/* skip, not sure of its use */
	read_packet_w(data, &cursor, len, &charset_code);

	tmp = g_strndup((gchar *) cursor, data + len - cursor);
	font_name = qq_to_utf8(tmp, QQ_CHARSET_DEFAULT);
	g_free(tmp);

	font_size = _get_size(font_attr);
	is_bold = _check_bold(font_attr);
	is_italic = _check_italic(font_attr);
	is_underline = _check_underline(font_attr);

	/* Although there is charset returned from QQ msg, it can't be used.
	 * For example, if a user send a Chinese message from English Windows
	 * the charset_code in QQ msg is 0x0000, not 0x8602.
	 * Therefore, it is better to use uniform conversion.
	 * By default, we use GBK, which includes all character of SC, TC, and EN. */
	msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
	encoded = g_string_new("");

	/* Henry: The range QQ sends rounds from 8 to 22, where a font size
	 * of 10 is equal to 3 in html font tag */
	g_string_append_printf(encoded,
			       "<font color=\"%s\"><font face=\"%s\"><font size=\"%d\">",
			       color_code, font_name, font_size / 3);
	gaim_debug(GAIM_DEBUG_INFO, "QQ_MESG",
		   "recv <font color=\"%s\"><font face=\"%s\"><font size=\"%d\">\n",
		   color_code, font_name, font_size / 3);
	g_string_append(encoded, msg_utf8);

	if (is_bold) {
		g_string_prepend(encoded, "<b>");
		g_string_append(encoded, "</b>");
	}
	if (is_italic) {
		g_string_prepend(encoded, "<i>");
		g_string_append(encoded, "</i>");
	}
	if (is_underline) {
		g_string_prepend(encoded, "<u>");
		g_string_append(encoded, "</u>");
	}

	g_string_append(encoded, "</font></font></font>");
	ret = encoded->str;

	g_free(msg_utf8);
	g_free(font_name);
	g_free(color_code);
	g_string_free(encoded, FALSE);

	return ret;
}

/* two convenience methods, using _my_convert */
gchar *utf8_to_qq(const gchar *str, const gchar *to_charset)
{
	return _my_convert(str, -1, to_charset, UTF8);
}

gchar *qq_to_utf8(const gchar *str, const gchar *from_charset)
{
	return _my_convert(str, -1, UTF8, from_charset);
}

/* QQ uses binary code for smiley, while gaim uses strings. 
 * There is a mapping relation between these two. */
gchar *qq_smiley_to_gaim(gchar *text)
{
	gint index;
	gchar qq_smiley, *cur_seg, **segments, *ret;
	GString *converted;

	converted = g_string_new("");
	segments = split_data((guint8 *) text, strlen(text), "\x14", 0);
	g_string_append(converted, segments[0]);

	while ((*(++segments)) != NULL) {
		cur_seg = *segments;
		qq_smiley = cur_seg[0];
		for (index = 0; index < QQ_SMILEY_AMOUNT; index++) {
			if (qq_smiley_map[index] == qq_smiley)
				break;
		}
		if (index >= QQ_SMILEY_AMOUNT) {
			g_string_append(converted, QQ_NULL_SMILEY);
		} else {
			g_string_append(converted, gaim_smiley_map[index]);
			g_string_append(converted, (cur_seg + 1));
		}
	}

	ret = converted->str;
	g_string_free(converted, FALSE);
	return ret;
}

/* convert smiley from gaim style to qq binary code */
gchar *gaim_smiley_to_qq(gchar *text)
{
	gchar *begin, *cursor, *ret;
	gint index;
	GString *converted;

	converted = g_string_new(text);

	for (index = 0; index < QQ_SMILEY_AMOUNT; index++) {
		begin = cursor = converted->str;
		while ((cursor = g_strstr_len(cursor, -1, gaim_smiley_map[index]))) {
			g_string_erase(converted, (cursor - begin), strlen(gaim_smiley_map[index]));
			g_string_insert_c(converted, (cursor - begin), 0x14);
			g_string_insert_c(converted, (cursor - begin + 1), qq_smiley_map[index]);
			cursor++;
		}
	}
	g_string_append_c(converted, 0x20);	/* important for last smiiley */

	ret = converted->str;
	g_string_free(converted, FALSE);
	return ret;
}
