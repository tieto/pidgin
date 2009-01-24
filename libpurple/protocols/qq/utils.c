/**
 * @file utils.c
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

#include "limits.h"
#include "stdlib.h"
#include "string.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#include "cipher.h"

#include "char_conv.h"
#include "debug.h"
#include "prefs.h"
#include "qq.h"
#include "util.h"
#include "utils.h"

/* These functions are used only in development phase */
/*
   static void _qq_show_socket(gchar *desc, gint fd) {
   struct sockaddr_in sin;
   socklen_t len = sizeof(sin);
   getsockname(fd, (struct sockaddr *)&sin, &len);
   purple_debug_info(desc, "%s:%d\n",
			inet_ntoa(sin.sin_addr), g_ntohs(sin.sin_port));
   }
   */

void qq_get_md5(guint8 *md5, gint md5_len, const guint8* const src, gint src_len)
{
	PurpleCipher *cipher;
	PurpleCipherContext *context;

	g_return_if_fail(md5 != NULL && md5_len > 0);
	g_return_if_fail(src != NULL && src_len > 0);

	cipher = purple_ciphers_find_cipher("md5");
	context = purple_cipher_context_new(cipher, NULL);
	purple_cipher_context_append(context, src, src_len);
	purple_cipher_context_digest(context, md5_len, md5, NULL);
	purple_cipher_context_destroy(context);
}

gchar *get_name_by_index_str(gchar **array, const gchar *index_str, gint amount)
{
	gint index;

	index = atoi(index_str);
	if (index < 0 || index >= amount)
		index = 0;

	return array[index];
}

gchar *get_index_str_by_name(gchar **array, const gchar *name, gint amount)
{
	gint index;

	for (index = 0; index <= amount; index++)
		if (g_ascii_strcasecmp(array[index], name) == 0)
			break;

	if (index >= amount)
		index = 0;	/* meaning no match */
	return g_strdup_printf("%d", index);
}

/* split the given data(len) with delimit,
 * check the number of field matches the expected_fields (<=0 means all)
 * return gchar* array (needs to be freed by g_strfreev later), or NULL */
gchar **split_data(guint8 *data, gint len, const gchar *delimit, gint expected_fields)
{
	guint8 *input;
	gchar **segments, **seg;
	gint count = 0, j;

	g_return_val_if_fail(data != NULL && len != 0 && delimit != 0, NULL);

	/* as the last field would be string, but data is not ended with 0x00
	 * we have to duplicate the data and append a 0x00 at the end */
	input = g_newa(guint8, len + 1);
	g_memmove(input, data, len);
	input[len] = 0x00;

	segments = g_strsplit((gchar *) input, delimit, 0);
	if (expected_fields <= 0)
		return segments;

	for (seg = segments; *seg != NULL; seg++)
		count++;
	if (count < expected_fields) {	/* not enough fields */
		purple_debug_error("QQ", "Less fields %d then %d\n", count, expected_fields);
		return NULL;
	} else if (count > expected_fields) {	/* more fields, OK */
		purple_debug_warning("QQ", "More fields %d than %d\n", count, expected_fields);
		/* free up those not used */
		for (j = expected_fields; j < count; j++) {
			purple_debug_warning("QQ", "field[%d] is %s\n", j, segments[j]);
			g_free(segments[j]);
		}
		segments[expected_fields] = NULL;
	}

	return segments;
}

/* convert Purple name to original QQ UID */
guint32 purple_name_to_uid(const gchar *const name)
{
	guint32 ret;
	g_return_val_if_fail(name != NULL, 0);

	ret = strtoul(name, NULL, 10);
	if (errno == ERANGE)
		return 0;
	else
		return ret;
}

gchar *gen_ip_str(guint8 *ip) {
	gchar *ret;
	if (ip == NULL || ip[0] == 0) {
		ret = g_new(gchar, 1);
		*ret = '\0';
		return ret;
	} else {
		return g_strdup_printf("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	}
}

guint8 *str_ip_gen(gchar *str) {
	guint8 *ip = g_new(guint8, 4);
	gint a, b, c, d;

	sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d);
	ip[0] = a;
	ip[1] = b;
	ip[2] = c;
	ip[3] = d;
	return ip;
}

/* convert a QQ UID to a unique name of Purple
 * the return needs to be freed */
gchar *uid_to_purple_name(guint32 uid)
{
	return g_strdup_printf("%u", uid);
}

/* try to dump the data as GBK */
gchar* try_dump_as_gbk(const guint8 *const data, gint len)
{
	gint i;
	guint8 *incoming;
	gchar *msg_utf8;

	incoming = g_newa(guint8, len + 1);
	g_memmove(incoming, data, len);
	incoming[len] = 0x00;
	/* GBK code:
	 * Single-byte ASCII:      0x21-0x7E
	 * GBK first byte range:   0x81-0xFE
	 * GBK second byte range:  0x40-0x7E and 0x80-0xFE */
	for (i = 0; i < len; i++)
		if (incoming[i] >= 0x81)
			break;

	msg_utf8 = i < len ? qq_to_utf8((gchar *) &incoming[i], QQ_CHARSET_DEFAULT) : NULL;

	if (msg_utf8 != NULL) {
		purple_debug_warning("QQ", "Try extract GB msg: %s\n", msg_utf8);
	}
	return msg_utf8;
}

/* strips whitespace */
static gchar *strstrip(const gchar *const buffer)
{
	GString *stripped;
	gchar *ret, cur;
	gint i;

	g_return_val_if_fail(buffer != NULL, NULL);

	stripped = g_string_new("");
	for (i=0; i<strlen(buffer); i++) {
		cur = buffer[i];
		if (cur != ' ' && cur != '\n')
			g_string_append_c(stripped, buffer[i]);
	}
	ret = stripped->str;
	g_string_free(stripped, FALSE);

	return ret;
}

/* Attempts to dump an ASCII hex string to a string of bytes.
 * The return should be freed later. */
guint8 *hex_str_to_bytes(const gchar *const buffer, gint *out_len)
{
	gchar *hex_str, *hex_buffer, *cursor, tmp;
	guint8 *bytes, nibble1, nibble2;
	gint index;

	g_return_val_if_fail(buffer != NULL, NULL);

	hex_buffer = strstrip(buffer);

	if (strlen(hex_buffer) % 2 != 0) {
		purple_debug_warning("QQ",
			"Unable to convert an odd number of nibbles to a string of bytes!\n");
		g_free(hex_buffer);
		return NULL;
	}
	bytes = g_newa(guint8, strlen(hex_buffer) / 2);
	hex_str = g_ascii_strdown(hex_buffer, -1);
	g_free(hex_buffer);
	index = 0;
	for (cursor = hex_str; cursor < hex_str + sizeof(gchar) * (strlen(hex_str)) - 1; cursor++) {
		if (g_ascii_isdigit(*cursor)) {
			tmp = *cursor; nibble1 = atoi(&tmp);
		} else if (g_ascii_isalpha(*cursor) && (gint) *cursor - 87 < 16) {
			nibble1 = (gint) *cursor - 87;
		} else {
			purple_debug_warning("QQ", "Invalid char \'%c\' found in hex string!\n",
					*cursor);
			g_free(hex_str);
			return NULL;
		}
		nibble1 = nibble1 << 4;
		cursor++;
		if (g_ascii_isdigit(*cursor)) {
			tmp = *cursor; nibble2 = atoi(&tmp);
		} else if (g_ascii_isalpha(*cursor) && (gint) (*cursor - 87) < 16) {
			nibble2 = (gint) *cursor - 87;
		} else {
			purple_debug_warning("QQ", "Invalid char found in hex string!\n");
			g_free(hex_str);
			return NULL;
		}
		bytes[index++] = nibble1 + nibble2;
	}
	*out_len = strlen(hex_str) / 2;
	g_free(hex_str);
	return g_memdup(bytes, *out_len);
}

/* Dumps a chunk of raw data into an ASCII hex string.
 * The return should be freed later. */
static gchar *hex_dump_to_str(const guint8 *const buffer, gint bytes)
{
	GString *str;
	gchar *ret;
	gint i, j, ch;

	str = g_string_new("");
	for (i = 0; i < bytes; i += 16) {
		/* length label */
		g_string_append_printf(str, "%07x: ", i);

		/* dump hex value */
		for (j = 0; j < 16; j++)
			if ((i + j) < bytes)
				g_string_append_printf(str, " %02x", buffer[i + j]);
			else
				g_string_append(str, " --");
		g_string_append(str, "  ");

		/* dump ascii value */
		for (j = 0; j < 16 && (i + j) < bytes; j++) {
			ch = buffer[i + j] & 127;
			if (ch < ' ' || ch == 127)
				g_string_append_c(str, '.');
			else
				g_string_append_c(str, ch);
		}
		g_string_append_c(str, '\n');
	}

	ret = str->str;
	/* GString can be freed without freeing it character data */
	g_string_free(str, FALSE);

	return ret;
}

void qq_hex_dump(PurpleDebugLevel level, const char *category,
		const guint8 *pdata, gint bytes,
		const char *format, ...)
{
	va_list args;
	char *arg_s = NULL;
	gchar *phex = NULL;

	g_return_if_fail(level != PURPLE_DEBUG_ALL);
	g_return_if_fail(format != NULL);

	va_start(args, format);
	arg_s = g_strdup_vprintf(format, args);
	va_end(args);

	if (bytes <= 0) {
		purple_debug(level, category, "%s", arg_s);
		return;
	}

	phex = hex_dump_to_str(pdata, bytes);
	purple_debug(level, category, "%s - (len %d)\n%s", arg_s, bytes, phex);
	g_free(phex);
}

void qq_show_packet(const gchar *desc, const guint8 *buf, gint len)
{
	qq_hex_dump(PURPLE_DEBUG_WARNING, "QQ", buf, len, desc);
}

void qq_filter_str(gchar *str) {
	gchar *temp;
	if (str == NULL) {
		return;
	}

	for (temp = str; *temp != 0; temp++) {
		/*if (*temp == '\r' || *temp == '\n')  *temp = ' ';*/
		if (*temp > 0 && *temp < 0x20)  *temp = ' ';
	}
	g_strstrip(str);
}
