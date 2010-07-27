/*
 * Purple's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
*/

#include "encoding.h"

guint16
oscar_charset_check(const char *utf8)
{
	while (*utf8++)
	{
		if ((unsigned char)(*utf8) > 0x7f) {
			/* not ASCII! */
			return AIM_CHARSET_UNICODE;
		}
	}
	return AIM_CHARSET_ASCII;
}

gchar *
oscar_encoding_extract(const char *encoding)
{
	gchar *ret = NULL;
	char *begin, *end;

	g_return_val_if_fail(encoding != NULL, NULL);

	/* Make sure encoding begins with charset= */
	if (strncmp(encoding, "text/aolrtf; charset=", 21) &&
		strncmp(encoding, "text/x-aolrtf; charset=", 23) &&
		strncmp(encoding, "text/plain; charset=", 20))
	{
		return NULL;
	}

	begin = strchr(encoding, '"');
	end = strrchr(encoding, '"');

	if ((begin == NULL) || (end == NULL) || (begin >= end))
		return NULL;

	ret = g_strndup(begin+1, (end-1) - begin);

	return ret;
}

gchar *
oscar_encoding_to_utf8(PurpleAccount *account, const char *encoding, const char *text, int textlen)
{
	gchar *utf8 = NULL;

	if ((encoding == NULL) || encoding[0] == '\0') {
		purple_debug_info("oscar", "Empty encoding, assuming UTF-8\n");
	} else if (!g_ascii_strcasecmp(encoding, "iso-8859-1")) {
		utf8 = g_convert(text, textlen, "UTF-8", "iso-8859-1", NULL, NULL, NULL);
	} else if (!g_ascii_strcasecmp(encoding, "ISO-8859-1-Windows-3.1-Latin-1") ||
	           !g_ascii_strcasecmp(encoding, "us-ascii"))
	{
		utf8 = g_convert(text, textlen, "UTF-8", "Windows-1252", NULL, NULL, NULL);
	} else if (!g_ascii_strcasecmp(encoding, "unicode-2-0")) {
		/* Some official ICQ clients are apparently total crack,
		 * and have been known to save a UTF-8 string converted
		 * from the locale character set to UTF-16 (not from UTF-8
		 * to UTF-16!) in the away message.  This hack should find
		 * and do something (un)reasonable with that, and not
		 * mess up too much else. */
		const gchar *charset = purple_account_get_string(account, "encoding", NULL);
		if (charset) {
			gsize len;
			utf8 = g_convert(text, textlen, charset, "UTF-16BE", &len, NULL, NULL);
			if (!utf8 || len != textlen || !g_utf8_validate(utf8, -1, NULL)) {
				g_free(utf8);
				utf8 = NULL;
			} else {
				purple_debug_info("oscar", "Used broken ICQ fallback encoding\n");
			}
		}
		if (!utf8)
			utf8 = g_convert(text, textlen, "UTF-8", "UTF-16BE", NULL, NULL, NULL);
	} else if (g_ascii_strcasecmp(encoding, "utf-8")) {
		purple_debug_warning("oscar", "Unrecognized character encoding \"%s\", "
						   "attempting to convert to UTF-8 anyway\n", encoding);
		utf8 = g_convert(text, textlen, "UTF-8", encoding, NULL, NULL, NULL);
	}

	/*
	 * If utf8 is still NULL then either the encoding is utf-8 or
	 * we have been unable to convert the text to utf-8 from the encoding
	 * that was specified.  So we check if the text is valid utf-8 then
	 * just copy it.
	 */
	if (utf8 == NULL) {
		if (textlen != 0 && *text != '\0'
				&& !g_utf8_validate(text, textlen, NULL))
			utf8 = g_strdup(_("(There was an error receiving this message.  The buddy you are speaking with is probably using a different encoding than expected.  If you know what encoding he is using, you can specify it in the advanced account options for your AIM/ICQ account.)"));
		else
			utf8 = g_strndup(text, textlen);
	}

	return utf8;
}

gchar *
oscar_utf8_try_convert(PurpleAccount *account, OscarData *od, const gchar *msg)
{
	const char *charset = NULL;
	char *ret = NULL;

	if (od->icq)
		charset = purple_account_get_string(account, "encoding", NULL);

	if(charset && *charset)
		ret = g_convert(msg, -1, "UTF-8", charset, NULL, NULL, NULL);

	if(!ret)
		ret = purple_utf8_try_convert(msg);

	return ret;
}

static gchar *
oscar_convert_to_utf8(const gchar *data, gsize datalen, const char *charsetstr, gboolean fallback)
{
	gchar *ret = NULL;
	GError *err = NULL;

	if ((charsetstr == NULL) || (*charsetstr == '\0'))
		return NULL;

	if (g_ascii_strcasecmp("UTF-8", charsetstr)) {
		if (fallback)
			ret = g_convert_with_fallback(data, datalen, "UTF-8", charsetstr, "?", NULL, NULL, &err);
		else
			ret = g_convert(data, datalen, "UTF-8", charsetstr, NULL, NULL, &err);
		if (err != NULL) {
			purple_debug_warning("oscar", "Conversion from %s failed: %s.\n",
							   charsetstr, err->message);
			g_error_free(err);
		}
	} else {
		if (g_utf8_validate(data, datalen, NULL))
			ret = g_strndup(data, datalen);
		else
			purple_debug_warning("oscar", "String is not valid UTF-8.\n");
	}

	return ret;
}

gchar *
oscar_decode_im_part(PurpleAccount *account, const char *sourcebn, guint16 charset, guint16 charsubset, const gchar *data, gsize datalen)
{
	gchar *ret = NULL;
	/* charsetstr1 is always set to what the correct encoding should be. */
	const gchar *charsetstr1, *charsetstr2, *charsetstr3 = NULL;

	if ((datalen == 0) || (data == NULL))
		return NULL;

	if (charset == AIM_CHARSET_UNICODE) {
		charsetstr1 = "UTF-16BE";
		charsetstr2 = "UTF-8";
	} else if (charset == AIM_CHARSET_LATIN_1) {
		if ((sourcebn != NULL) && oscar_util_valid_name_icq(sourcebn))
			charsetstr1 = purple_account_get_string(account, "encoding", OSCAR_DEFAULT_CUSTOM_ENCODING);
		else
			charsetstr1 = "ISO-8859-1";
		charsetstr2 = "UTF-8";
	} else if (charset == AIM_CHARSET_ASCII) {
		/* Should just be "ASCII" */
		charsetstr1 = "ASCII";
		charsetstr2 = purple_account_get_string(account, "encoding", OSCAR_DEFAULT_CUSTOM_ENCODING);
	} else if (charset == 0x000d) {
		/* iChat sending unicode over a Direct IM connection = UTF-8 */
		/* Mobile AIM client on multiple devices (including Blackberry Tour, Nokia 3100, and LG VX6000) = ISO-8859-1 */
		charsetstr1 = "UTF-8";
		charsetstr2 = "ISO-8859-1";
		charsetstr3 = purple_account_get_string(account, "encoding", OSCAR_DEFAULT_CUSTOM_ENCODING);
	} else {
		/* Unknown, hope for valid UTF-8... */
		charsetstr1 = "UTF-8";
		charsetstr2 = purple_account_get_string(account, "encoding", OSCAR_DEFAULT_CUSTOM_ENCODING);
	}

	purple_debug_info("oscar", "Parsing IM part, charset=0x%04hx, charsubset=0x%04hx, datalen=%" G_GSIZE_FORMAT ", choice1=%s, choice2=%s, choice3=%s\n",
					  charset, charsubset, datalen, charsetstr1, charsetstr2, (charsetstr3 ? charsetstr3 : ""));

	ret = oscar_convert_to_utf8(data, datalen, charsetstr1, FALSE);
	if (ret == NULL) {
		if (charsetstr3 != NULL) {
			/* Try charsetstr2 without allowing substitutions, then fall through to charsetstr3 if needed */
			ret = oscar_convert_to_utf8(data, datalen, charsetstr2, FALSE);
			if (ret == NULL)
				ret = oscar_convert_to_utf8(data, datalen, charsetstr3, TRUE);
		} else {
			/* Try charsetstr2, allowing substitutions */
			ret = oscar_convert_to_utf8(data, datalen, charsetstr2, TRUE);
		}
	}
	if (ret == NULL) {
		char *str, *salvage, *tmp;

		str = g_malloc(datalen + 1);
		strncpy(str, data, datalen);
		str[datalen] = '\0';
		salvage = purple_utf8_salvage(str);
		tmp = g_strdup_printf(_("(There was an error receiving this message.  Either you and %s have different encodings selected, or %s has a buggy client.)"),
					  sourcebn, sourcebn);
		ret = g_strdup_printf("%s %s", salvage, tmp);
		g_free(tmp);
		g_free(str);
		g_free(salvage);
	}

	return ret;
}

gchar *
oscar_convert_to_best_encoding(const gchar *msg, gsize *result_len, guint16 *charset, gchar **charsetstr)
{
	guint16 msg_charset = oscar_charset_check(msg);
	if (charset != NULL) {
		*charset = msg_charset;
	}
	if (charsetstr != NULL) {
		*charsetstr = msg_charset == AIM_CHARSET_ASCII ? "us-ascii" : "unicode-2-0";
	}
	return g_convert(msg, -1, msg_charset == AIM_CHARSET_ASCII ? "ASCII" : "UTF-16BE", "UTF-8", NULL, result_len, NULL);
}
