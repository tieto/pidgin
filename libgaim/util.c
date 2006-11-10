/*
 * @file util.h Utility Functions
 * @ingroup core
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
#include "internal.h"

#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "prefs.h"
#include "util.h"

struct _GaimUtilFetchUrlData
{
	GaimUtilFetchUrlCallback callback;
	void *user_data;

	struct
	{
		char *user;
		char *passwd;
		char *address;
		int port;
		char *page;

	} website;

	char *url;
	gboolean full;
	char *user_agent;
	gboolean http11;
	char *request;
	gsize request_written;
	gboolean include_headers;

	GaimProxyConnectData *connect_data;
	int fd;
	guint inpa;

	gboolean got_headers;
	gboolean has_explicit_data_len;
	char *webdata;
	unsigned long len;
	unsigned long data_len;
};

static char custom_home_dir[MAXPATHLEN];
static char home_dir[MAXPATHLEN];

GaimMenuAction *
gaim_menu_action_new(const char *label, GaimCallback callback, gpointer data,
                     GList *children)
{
	GaimMenuAction *act = g_new0(GaimMenuAction, 1);
	act->label = g_strdup(label);
	act->callback = callback;
	act->data = data;
	act->children = children;
	return act;
}

void
gaim_menu_action_free(GaimMenuAction *act)
{
	g_return_if_fail(act != NULL);

	g_free(act->label);
	g_free(act);
}

/**************************************************************************
 * Base16 Functions
 **************************************************************************/
gchar *
gaim_base16_encode(const guchar *data, gsize len)
{
	int i;
	gchar *ascii = NULL;

	g_return_val_if_fail(data != NULL, NULL);
	g_return_val_if_fail(len > 0,   NULL);

	ascii = g_malloc(len * 2 + 1);

	for (i = 0; i < len; i++)
		snprintf(&ascii[i * 2], 3, "%02hhx", data[i]);

	return ascii;
}

guchar *
gaim_base16_decode(const char *str, gsize *ret_len)
{
	int len, i, accumulator = 0;
	guchar *data;

	g_return_val_if_fail(str != NULL, NULL);

	len = strlen(str);

	g_return_val_if_fail(strlen(str) > 0, 0);
	g_return_val_if_fail(len % 2 > 0,       0);

	data = g_malloc(len / 2);

	for (i = 0; i < len; i++)
	{
		if ((i % 2) == 0)
			accumulator = 0;
		else
			accumulator <<= 4;

		if (isdigit(str[i]))
			accumulator |= str[i] - 48;
		else
		{
			switch(str[i])
			{
				case 'a':  case 'A':  accumulator |= 10;  break;
				case 'b':  case 'B':  accumulator |= 11;  break;
				case 'c':  case 'C':  accumulator |= 12;  break;
				case 'd':  case 'D':  accumulator |= 13;  break;
				case 'e':  case 'E':  accumulator |= 14;  break;
				case 'f':  case 'F':  accumulator |= 15;  break;
			}
		}

		if (i % 2)
			data[(i - 1) / 2] = accumulator;
	}

	if (ret_len != NULL)
		*ret_len = len / 2;

	return data;
}

/**************************************************************************
 * Base64 Functions
 **************************************************************************/
static const char alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

static const char xdigits[] =
	"0123456789abcdef";

gchar *
gaim_base64_encode(const guchar *data, gsize len)
{
	char *out, *rv;

	g_return_val_if_fail(data != NULL, NULL);
	g_return_val_if_fail(len > 0,  NULL);

	rv = out = g_malloc(((len/3)+1)*4 + 1);

	for (; len >= 3; len -= 3)
	{
		*out++ = alphabet[data[0] >> 2];
		*out++ = alphabet[((data[0] << 4) & 0x30) | (data[1] >> 4)];
		*out++ = alphabet[((data[1] << 2) & 0x3c) | (data[2] >> 6)];
		*out++ = alphabet[data[2] & 0x3f];
		data += 3;
	}

	if (len > 0)
	{
		unsigned char fragment;

		*out++ = alphabet[data[0] >> 2];
		fragment = (data[0] << 4) & 0x30;

		if (len > 1)
			fragment |= data[1] >> 4;

		*out++ = alphabet[fragment];
		*out++ = (len < 2) ? '=' : alphabet[(data[1] << 2) & 0x3c];
		*out++ = '=';
	}

	*out = '\0';

	return rv;
}

guchar *
gaim_base64_decode(const char *str, gsize *ret_len)
{
	guchar *out = NULL;
	char tmp = 0;
	const char *c;
	gint32 tmp2 = 0;
	int len = 0, n = 0;

	g_return_val_if_fail(str != NULL, NULL);

	c = str;

	while (*c) {
		if (*c >= 'A' && *c <= 'Z') {
			tmp = *c - 'A';
		} else if (*c >= 'a' && *c <= 'z') {
			tmp = 26 + (*c - 'a');
		} else if (*c >= '0' && *c <= 57) {
			tmp = 52 + (*c - '0');
		} else if (*c == '+') {
			tmp = 62;
		} else if (*c == '/') {
			tmp = 63;
		} else if (*c == '\r' || *c == '\n') {
			c++;
			continue;
		} else if (*c == '=') {
			if (n == 3) {
				out = g_realloc(out, len + 2);
				out[len] = (guchar)(tmp2 >> 10) & 0xff;
				len++;
				out[len] = (guchar)(tmp2 >> 2) & 0xff;
				len++;
			} else if (n == 2) {
				out = g_realloc(out, len + 1);
				out[len] = (guchar)(tmp2 >> 4) & 0xff;
				len++;
			}
			break;
		}
		tmp2 = ((tmp2 << 6) | (tmp & 0xff));
		n++;
		if (n == 4) {
			out = g_realloc(out, len + 3);
			out[len] = (guchar)((tmp2 >> 16) & 0xff);
			len++;
			out[len] = (guchar)((tmp2 >> 8) & 0xff);
			len++;
			out[len] = (guchar)(tmp2 & 0xff);
			len++;
			tmp2 = 0;
			n = 0;
		}
		c++;
	}

	out = g_realloc(out, len + 1);
	out[len] = 0;

	if (ret_len != NULL)
		*ret_len = len;

	return out;
}

/**************************************************************************
 * Quoted Printable Functions (see RFC 2045).
 **************************************************************************/
guchar *
gaim_quotedp_decode(const char *str, gsize *ret_len)
{
	char *n, *new;
	const char *end, *p;

	n = new = g_malloc(strlen (str) + 1);
	end = str + strlen(str);

	for (p = str; p < end; p++, n++) {
		if (*p == '=') {
			if (p[1] == '\r' && p[2] == '\n') { /* 5.1 #5 */
				n -= 1;
				p += 2;
			} else if (p[1] == '\n') { /* fuzzy case for 5.1 #5 */
				n -= 1;
				p += 1;
			} else if (p[1] && p[2]) {
				char *nibble1 = strchr(xdigits, tolower(p[1]));
				char *nibble2 = strchr(xdigits, tolower(p[2]));
				if (nibble1 && nibble2) { /* 5.1 #1 */
					*n = ((nibble1 - xdigits) << 4) | (nibble2 - xdigits);
					p += 2;
				} else { /* This should never happen */
					*n = *p;
				}
			} else { /* This should never happen */
				*n = *p;
			}
		}
		else if (*p == '_')
			*n = ' ';
		else
			*n = *p;
	}

	*n = '\0';

	if (ret_len != NULL)
		*ret_len = n - new;

	/* Resize to take less space */
	/* new = realloc(new, n - new); */

	return (guchar *)new;
}

/**************************************************************************
 * MIME Functions
 **************************************************************************/
char *
gaim_mime_decode_field(const char *str)
{
	/*
	 * This is wing's version, partially based on revo/shx's version
	 * See RFC2047 [which apparently obsoletes RFC1342]
	 */
	typedef enum {
		state_start, state_equal1, state_question1,
		state_charset, state_question2,
		state_encoding, state_question3,
		state_encoded_text, state_question4, state_equal2 = state_start
	} encoded_word_state_t;
	encoded_word_state_t state = state_start;
	const char *cur, *mark;
	const char *charset0 = NULL, *encoding0 = NULL, *encoded_text0 = NULL;
	char *n, *new;

	/* token can be any CHAR (supposedly ISO8859-1/ISO2022), not just ASCII */
	#define token_char_p(c) \
		(c != ' ' && !iscntrl(c) && !strchr("()<>@,;:\"/[]?.=", c))

	/* But encoded-text must be ASCII; alas, isascii() may not exist */
	#define encoded_text_char_p(c) \
		((c & 0x80) == 0 && c != '?' && c != ' ' && isgraph(c))

	#define RECOVER_MARKED_TEXT strncpy(n, mark, cur - mark + 1); \
		n += cur - mark + 1

	g_return_val_if_fail(str != NULL, NULL);

	/* NOTE: Assuming that we need just strlen(str)+1 *may* be wrong.
	 * It would be wrong if one byte (in some unknown encoding) could
	 * expand to >=4 bytes of UTF-8; I don't know if there are such things.
	 */
	n = new = g_malloc(strlen(str) + 1);

	/* Here we will be looking for encoded words and if they seem to be
	 * valid then decode them.
	 * They are of this form: =?charset?encoding?text?=
	 */

	for (cur = str, mark = NULL; *cur; cur += 1) {
		switch (state) {
		case state_equal1:
			if (*cur == '?') {
				state = state_question1;
			} else {
				RECOVER_MARKED_TEXT;
				state = state_start;
			}
			break;
		case state_question1:
			if (token_char_p(*cur)) {
				charset0 = cur;
				state = state_charset;
			} else { /* This should never happen */
				RECOVER_MARKED_TEXT;
				state = state_start;
			}
			break;
		case state_charset:
			if (*cur == '?') {
				state = state_question2;
			} else if (!token_char_p(*cur)) { /* This should never happen */
				RECOVER_MARKED_TEXT;
				state = state_start;
			}
			break;
		case state_question2:
			if (token_char_p(*cur)) {
				encoding0 = cur;
				state = state_encoding;
			} else { /* This should never happen */
				RECOVER_MARKED_TEXT;
				state = state_start;
			}
			break;
		case state_encoding:
			if (*cur == '?') {
				state = state_question3;
			} else if (!token_char_p(*cur)) { /* This should never happen */
				RECOVER_MARKED_TEXT;
				state = state_start;
			}
			break;
		case state_question3:
			if (encoded_text_char_p(*cur)) {
				encoded_text0 = cur;
				state = state_encoded_text;
			} else if (*cur == '?') { /* empty string */
				encoded_text0 = cur;
				state = state_question4;
			} else { /* This should never happen */
				RECOVER_MARKED_TEXT;
				state = state_start;
			}
			break;
		case state_encoded_text:
			if (*cur == '?') {
				state = state_question4;
			} else if (!encoded_text_char_p(*cur)) {
				RECOVER_MARKED_TEXT;
				state = state_start;
			}
			break;
		case state_question4:
			if (*cur == '=') { /* Got the whole encoded-word */
				char *charset = g_strndup(charset0, encoding0 - charset0 - 1);
				char *encoding = g_strndup(encoding0, encoded_text0 - encoding0 - 1);
				char *encoded_text = g_strndup(encoded_text0, cur - encoded_text0 - 1);
				guchar *decoded = NULL;
				gsize dec_len;
				if (g_ascii_strcasecmp(encoding, "Q") == 0)
					decoded = gaim_quotedp_decode(encoded_text, &dec_len);
				else if (g_ascii_strcasecmp(encoding, "B") == 0)
					decoded = gaim_base64_decode(encoded_text, &dec_len);
				else
					decoded = NULL;
				if (decoded) {
					gsize len;
					char *converted = g_convert((const gchar *)decoded, dec_len, "utf-8", charset, NULL, &len, NULL);

					if (converted) {
						n = strncpy(n, converted, len) + len;
						g_free(converted);
					}
					g_free(decoded);
				}
				g_free(charset);
				g_free(encoding);
				g_free(encoded_text);
				state = state_equal2; /* Restart the FSM */
			} else { /* This should never happen */
				RECOVER_MARKED_TEXT;
				state = state_start;
			}
			break;
		default:
			if (*cur == '=') {
				mark = cur;
				state = state_equal1;
			} else {
				/* Some unencoded text. */
				*n = *cur;
				n += 1;
			}
			break;
		} /* switch */
	} /* for */

	if (state != state_start) {
		RECOVER_MARKED_TEXT;
	}
	*n = '\0';

	return new;
}


/**************************************************************************
 * Date/Time Functions
 **************************************************************************/

#ifdef _WIN32
static long win32_get_tz_offset() {
	TIME_ZONE_INFORMATION tzi;
	DWORD ret;
	long off = -1;

	if ((ret = GetTimeZoneInformation(&tzi)) != TIME_ZONE_ID_INVALID)
	{
		off = -(tzi.Bias * 60);
		if (ret == TIME_ZONE_ID_DAYLIGHT)
			off -= tzi.DaylightBias * 60;
	}

	return off;
}
#endif

#ifndef HAVE_STRFTIME_Z_FORMAT
static const char *get_tmoff(const struct tm *tm)
{
	static char buf[6];
	long off;
	gint8 min;
	gint8 hrs;
	struct tm new_tm = *tm;

	mktime(&new_tm);

	if (new_tm.tm_isdst < 0)
		g_return_val_if_reached("");

#ifdef _WIN32
	if ((off = win32_get_tz_offset()) == -1)
		return "";
#else
# ifdef HAVE_TM_GMTOFF
	off = new_tm.tm_gmtoff;
# else
#  ifdef HAVE_TIMEZONE
	tzset();
	off = -timezone;
#  endif /* HAVE_TIMEZONE */
# endif /* !HAVE_TM_GMTOFF */
#endif /* _WIN32 */

	min = (off / 60) % 60;
	hrs = ((off / 60) - min) / 60;

	if (g_snprintf(buf, sizeof(buf), "%+03d%02d", hrs, ABS(min)) > 5)
		g_return_val_if_reached("");

	return buf;
}
#endif

/* Windows doesn't HAVE_STRFTIME_Z_FORMAT, but this seems clearer. -- rlaager */
#if !defined(HAVE_STRFTIME_Z_FORMAT) || defined(_WIN32)
static size_t gaim_internal_strftime(char *s, size_t max, const char *format, const struct tm *tm)
{
	const char *start;
	const char *c;
	char *fmt = NULL;

	/* Yes, this is checked in gaim_utf8_strftime(),
	 * but better safe than sorry. -- rlaager */
	g_return_val_if_fail(format != NULL, 0);

	/* This is fairly efficient, and it only gets
	 * executed on Windows or if the underlying
	 * system doesn't support the %z format string,
	 * for strftime() so I think it's good enough.
	 * -- rlaager */
	for (c = start = format; *c ; c++)
	{
		if (*c != '%')
			continue;

		c++;

#ifndef HAVE_STRFTIME_Z_FORMAT
		if (*c == 'z')
		{
			char *tmp = g_strdup_printf("%s%.*s%s",
			                            fmt ? fmt : "",
			                            c - start - 1,
			                            start,
			                            get_tmoff(tm));
			g_free(fmt);
			fmt = tmp;
			start = c + 1;
		}
#endif
#ifdef _WIN32
		if (*c == 'Z')
		{
			char *tmp = g_strdup_printf("%s%.*s%s",
			                            fmt ? fmt : "",
			                            c - start - 1,
			                            start,
			                            wgaim_get_timezone_abbreviation(tm));
			g_free(fmt);
			fmt = tmp;
			start = c + 1;
		}
#endif
	}

	if (fmt != NULL)
	{
		size_t ret;

		if (*start)
		{
			char *tmp = g_strconcat(fmt, start, NULL);
			g_free(fmt);
			fmt = tmp;
		}

		ret = strftime(s, max, fmt, tm);
		g_free(fmt);

		return ret;
	}

	return strftime(s, max, format, tm);
}
#else /* HAVE_STRFTIME_Z_FORMAT && !_WIN32 */
#define gaim_internal_strftime strftime
#endif

const char *
gaim_utf8_strftime(const char *format, const struct tm *tm)
{
	static char buf[128];
	char *locale;
	GError *err = NULL;
	int len;
	char *utf8;

	g_return_val_if_fail(format != NULL, NULL);

	if (tm == NULL)
	{
		time_t now = time(NULL);
		tm = localtime(&now);
	}

	locale = g_locale_from_utf8(format, -1, NULL, NULL, &err);
	if (err != NULL)
	{
		gaim_debug_error("util", "Format conversion failed in gaim_utf8_strftime(): %s", err->message);
		g_error_free(err);
		locale = g_strdup(format);
	}

	/* A return value of 0 is either an error (in
	 * which case, the contents of the buffer are
	 * undefined) or the empty string (in which
	 * case, no harm is done here). */
	if ((len = gaim_internal_strftime(buf, sizeof(buf), locale, tm)) == 0)
	{
		g_free(locale);
		return "";
	}

	g_free(locale);

	utf8 = g_locale_to_utf8(buf, len, NULL, NULL, &err);
	if (err != NULL)
	{
		gaim_debug_error("util", "Result conversion failed in gaim_utf8_strftime(): %s", err->message);
		g_error_free(err);
	}
	else
	{
		gaim_strlcpy(buf, utf8);
		g_free(utf8);
	}

	return buf;
}

const char *
gaim_date_format_short(const struct tm *tm)
{
	return gaim_utf8_strftime("%x", tm);
}

const char *
gaim_date_format_long(const struct tm *tm)
{
	return gaim_utf8_strftime(_("%x %X"), tm);
}

const char *
gaim_date_format_full(const struct tm *tm)
{
	return gaim_utf8_strftime("%c", tm);
}

const char *
gaim_time_format(const struct tm *tm)
{
	return gaim_utf8_strftime("%X", tm);
}

time_t
gaim_time_build(int year, int month, int day, int hour, int min, int sec)
{
	struct tm tm;

	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec >= 0 ? sec : time(NULL) % 60;

	return mktime(&tm);
}

time_t
gaim_str_to_time(const char *timestamp, gboolean utc,
                 struct tm *tm, long *tz_off, const char **rest)
{
	time_t retval = 0;
	struct tm *t;
	const char *c = timestamp;
	int year = 0;
	long tzoff = GAIM_NO_TZ_OFF;

	time(&retval);
	t = localtime(&retval);

	/* 4 digit year */
	if (sscanf(c, "%04d", &year) && year > 1900)
	{
		c += 4;
		if (*c == '-')
			c++;
		t->tm_year = year - 1900;
	}

	/* 2 digit month */
	if (!sscanf(c, "%02d", &t->tm_mon))
	{
		if (rest != NULL && *c != '\0')
			*rest = c;
		return 0;
	}
	c += 2;
	if (*c == '-' || *c == '/')
		c++;
	t->tm_mon -= 1;

	/* 2 digit day */
	if (!sscanf(c, "%02d", &t->tm_mday))
	{
		if (rest != NULL && *c != '\0')
			*rest = c;
		return 0;
	}
	c += 2;
	if (*c == '/')
	{
		c++;

		if (!sscanf(c, "%04d", &t->tm_year))
		{
			if (rest != NULL && *c != '\0')
				*rest = c;
			return 0;
		}
		t->tm_year -= 1900;
	}
	else if (*c == 'T' || *c == '.')
	{
		c++;
		/* we have more than a date, keep going */

		/* 2 digit hour */
		if ((sscanf(c, "%02d:%02d:%02d", &t->tm_hour, &t->tm_min, &t->tm_sec) == 3 && (c = c + 8)) ||
		    (sscanf(c, "%02d%02d%02d", &t->tm_hour, &t->tm_min, &t->tm_sec) == 3 && (c = c + 6)))
		{
			gboolean offset_positive = FALSE;
			int tzhrs;
			int tzmins;

			t->tm_isdst = -1;

			if (*c == '.' && *(c+1) >= '0' && *(c+1) <= '9') /* dealing with precision we don't care about */
				c += 4;
			if (*c == '+')
				offset_positive = TRUE;
			if (((*c == '+' || *c == '-') && (c = c + 1)) &&
			    ((sscanf(c, "%02d:%02d", &tzhrs, &tzmins) == 2 && (c = c + 5)) ||
			     (sscanf(c, "%02d%02d", &tzhrs, &tzmins) == 2 && (c = c + 4))))
			{
				tzoff = tzhrs*60*60 + tzmins*60;
				if (offset_positive)
					tzoff *= -1;
				/* We don't want the C library doing DST calculations
				 * if we know the UTC offset already. */
				t->tm_isdst = 0;
			}

			if (rest != NULL && *c != '\0')
			{
				if (*c == ' ')
					c++;
				if (*c != '\0')
					*rest = c;
			}

			if (tzoff != GAIM_NO_TZ_OFF || utc)
			{
#if defined(_WIN32)
				long sys_tzoff;
#endif

#if defined(_WIN32) || defined(HAVE_TM_GMTOFF) || defined (HAVE_TIMEZONE)
				if (tzoff == GAIM_NO_TZ_OFF)
					tzoff = 0;
#endif

#ifdef _WIN32
				if ((sys_tzoff = win32_get_tz_offset()) == -1)
					tzoff = GAIM_NO_TZ_OFF;
				else
					tzoff += sys_tzoff;
#else
#ifdef HAVE_TM_GMTOFF
				tzoff += t->tm_gmtoff;
#else
#	ifdef HAVE_TIMEZONE
				tzset();    /* making sure */
				tzoff -= timezone;
#	endif
#endif
#endif /* _WIN32 */
			}
		}
		else
		{
			if (rest != NULL && *c != '\0')
				*rest = c;
		}
	}

	if (tm != NULL)
	{
		*tm = *t;
		tm->tm_isdst = -1;
		mktime(tm);
	}

	retval = mktime(t);
	if (tzoff != GAIM_NO_TZ_OFF)
		retval += tzoff;

	if (tz_off != NULL)
		*tz_off = tzoff;

	return retval;
}

/**************************************************************************
 * Markup Functions
 **************************************************************************/

/* Returns a NULL-terminated string after unescaping an entity
 * (eg. &amp;, &lt; &#38 etc.) starting at s. Returns NULL on failure.*/
static const char *
detect_entity(const char *text, int *length)
{
	const char *pln;
	int len, pound;

	if (!text || *text != '&')
		return NULL;

#define IS_ENTITY(s)  (!g_ascii_strncasecmp(text, s, (len = sizeof(s) - 1)))

	if(IS_ENTITY("&amp;"))
		pln = "&";
	else if(IS_ENTITY("&lt;"))
		pln = "<";
	else if(IS_ENTITY("&gt;"))
		pln = ">";
	else if(IS_ENTITY("&nbsp;"))
		pln = " ";
	else if(IS_ENTITY("&copy;"))
		pln = "\302\251";      /* or use g_unichar_to_utf8(0xa9); */
	else if(IS_ENTITY("&quot;"))
		pln = "\"";
	else if(IS_ENTITY("&reg;"))
		pln = "\302\256";      /* or use g_unichar_to_utf8(0xae); */
	else if(IS_ENTITY("&apos;"))
		pln = "\'";
	else if(*(text+1) == '#' && (sscanf(text, "&#%u;", &pound) == 1) &&
			pound != 0 && *(text+3+(gint)log10(pound)) == ';') {
		static char buf[7];
		int buflen = g_unichar_to_utf8((gunichar)pound, buf);
		buf[buflen] = '\0';
		pln = buf;

		len = 2;
		while(isdigit((gint) text[len])) len++;
		if(text[len] == ';') len++;
	}
	else
		return NULL;

	if (length)
		*length = len;
	return pln;
}

gboolean
gaim_markup_find_tag(const char *needle, const char *haystack,
					 const char **start, const char **end, GData **attributes)
{
	GData *attribs;
	const char *cur = haystack;
	char *name = NULL;
	gboolean found = FALSE;
	gboolean in_tag = FALSE;
	gboolean in_attr = FALSE;
	const char *in_quotes = NULL;
	size_t needlelen;

	g_return_val_if_fail(    needle != NULL, FALSE);
	g_return_val_if_fail(   *needle != '\0', FALSE);
	g_return_val_if_fail(  haystack != NULL, FALSE);
	g_return_val_if_fail( *haystack != '\0', FALSE);
	g_return_val_if_fail(     start != NULL, FALSE);
	g_return_val_if_fail(       end != NULL, FALSE);
	g_return_val_if_fail(attributes != NULL, FALSE);

	needlelen = strlen(needle);
	g_datalist_init(&attribs);

	while (*cur && !found) {
		if (in_tag) {
			if (in_quotes) {
				const char *close = cur;

				while (*close && *close != *in_quotes)
					close++;

				/* if we got the close quote, store the value and carry on from    *
				 * after it. if we ran to the end of the string, point to the NULL *
				 * and we're outta here */
				if (*close) {
					/* only store a value if we have an attribute name */
					if (name) {
						size_t len = close - cur;
						char *val = g_strndup(cur, len);

						g_datalist_set_data_full(&attribs, name, val, g_free);
						g_free(name);
						name = NULL;
					}

					in_quotes = NULL;
					cur = close + 1;
				} else {
					cur = close;
				}
			} else if (in_attr) {
				const char *close = cur;

				while (*close && *close != '>' && *close != '"' &&
						*close != '\'' && *close != ' ' && *close != '=')
					close++;

				/* if we got the equals, store the name of the attribute. if we got
				 * the quote, save the attribute and go straight to quote mode.
				 * otherwise the tag closed or we reached the end of the string,
				 * so we can get outta here */
				switch (*close) {
				case '"':
				case '\'':
					in_quotes = close;
				case '=':
					{
						size_t len = close - cur;

						/* don't store a blank attribute name */
						if (len) {
							g_free(name);
							name = g_ascii_strdown(cur, len);
						}

						in_attr = FALSE;
						cur = close + 1;
						break;
					}
				case ' ':
				case '>':
					in_attr = FALSE;
				default:
					cur = close;
					break;
				}
			} else {
				switch (*cur) {
				case ' ':
					/* swallow extra spaces inside tag */
					while (*cur && *cur == ' ') cur++;
					in_attr = TRUE;
					break;
				case '>':
					found = TRUE;
					*end = cur;
					break;
				case '"':
				case '\'':
					in_quotes = cur;
				default:
					cur++;
					break;
				}
			}
		} else {
			/* if we hit a < followed by the name of our tag... */
			if (*cur == '<' && !g_ascii_strncasecmp(cur + 1, needle, needlelen)) {
				*start = cur;
				cur = cur + needlelen + 1;

				/* if we're pointing at a space or a >, we found the right tag. if *
				 * we're not, we've found a longer tag, so we need to skip to the  *
				 * >, but not being distracted by >s inside quotes.                */
				if (*cur == ' ' || *cur == '>') {
					in_tag = TRUE;
				} else {
					while (*cur && *cur != '"' && *cur != '\'' && *cur != '>') {
						if (*cur == '"') {
							cur++;
							while (*cur && *cur != '"')
								cur++;
						} else if (*cur == '\'') {
							cur++;
							while (*cur && *cur != '\'')
								cur++;
						} else {
							cur++;
						}
					}
				}
			} else {
				cur++;
			}
		}
	}

	/* clean up any attribute name from a premature termination */
	g_free(name);

	if (found) {
		*attributes = attribs;
	} else {
		*start = NULL;
		*end = NULL;
		*attributes = NULL;
	}

	return found;
}

gboolean
gaim_markup_extract_info_field(const char *str, int len, GString *dest,
							   const char *start_token, int skip,
							   const char *end_token, char check_value,
							   const char *no_value_token,
							   const char *display_name, gboolean is_link,
							   const char *link_prefix,
							   GaimInfoFieldFormatCallback format_cb)
{
	const char *p, *q;

	g_return_val_if_fail(str          != NULL, FALSE);
	g_return_val_if_fail(dest  != NULL, FALSE);
	g_return_val_if_fail(start_token  != NULL, FALSE);
	g_return_val_if_fail(end_token    != NULL, FALSE);
	g_return_val_if_fail(display_name != NULL, FALSE);

	p = strstr(str, start_token);

	if (p == NULL)
		return FALSE;

	p += strlen(start_token) + skip;

	if (p >= str + len)
		return FALSE;

	if (check_value != '\0' && *p == check_value)
		return FALSE;

	q = strstr(p, end_token);

	/* Trim leading blanks */
	while (*p != '\n' && g_ascii_isspace(*p)) {
		p += 1;
	}

	/* Trim trailing blanks */
	while (q > p && g_ascii_isspace(*(q - 1))) {
		q -= 1;
	}

	/* Don't bother with null strings */
	if (p == q)
		return FALSE;

	if (q != NULL && (!no_value_token ||
					  (no_value_token && strncmp(p, no_value_token,
												 strlen(no_value_token)))))
	{
		g_string_append_printf(dest, _("<b>%s:</b> "), display_name);

		if (is_link)
		{
			g_string_append(dest, "<br><a href=\"");

			if (link_prefix)
				g_string_append(dest, link_prefix);

			if (format_cb != NULL)
			{
				char *reformatted = format_cb(p, q - p);
				g_string_append(dest, reformatted);
				g_free(reformatted);
			}
			else
				g_string_append_len(dest, p, q - p);
			g_string_append(dest, "\">");

			if (link_prefix)
				g_string_append(dest, link_prefix);

			g_string_append_len(dest, p, q - p);
			g_string_append(dest, "</a>");
		}
		else
		{
			if (format_cb != NULL)
			{
				char *reformatted = format_cb(p, q - p);
				g_string_append(dest, reformatted);
				g_free(reformatted);
			}
			else
				g_string_append_len(dest, p, q - p);
		}

		g_string_append(dest, "<br>\n");

		return TRUE;
	}

	return FALSE;
}

struct gaim_parse_tag {
	char *src_tag;
	char *dest_tag;
	gboolean ignore;
};

#define ALLOW_TAG_ALT(x, y) if(!g_ascii_strncasecmp(c, "<" x " ", strlen("<" x " "))) { \
						const char *o = c + strlen("<" x); \
						const char *p = NULL, *q = NULL, *r = NULL; \
						GString *innards = g_string_new(""); \
						while(o && *o) { \
							if(!q && (*o == '\"' || *o == '\'') ) { \
								q = o; \
							} else if(q) { \
								if(*o == *q) { \
									char *unescaped = g_strndup(q+1, o-q-1); \
									char *escaped = g_markup_escape_text(unescaped, -1); \
									g_string_append_printf(innards, "%c%s%c", *q, escaped, *q); \
									g_free(unescaped); \
									g_free(escaped); \
									q = NULL; \
								} else if(*c == '\\') { \
									o++; \
								} \
							} else if(*o == '<') { \
								r = o; \
							} else if(*o == '>') { \
								p = o; \
								break; \
							} else { \
								innards = g_string_append_c(innards, *o); \
							} \
							o++; \
						} \
						if(p && !r) { \
							if(*(p-1) != '/') { \
								struct gaim_parse_tag *pt = g_new0(struct gaim_parse_tag, 1); \
								pt->src_tag = x; \
								pt->dest_tag = y; \
								tags = g_list_prepend(tags, pt); \
							} \
							xhtml = g_string_append(xhtml, "<" y); \
							c += strlen("<" x ); \
							xhtml = g_string_append(xhtml, innards->str); \
							xhtml = g_string_append_c(xhtml, '>'); \
							c = p + 1; \
						} else { \
							xhtml = g_string_append(xhtml, "&lt;"); \
							plain = g_string_append_c(plain, '<'); \
							c++; \
						} \
						g_string_free(innards, TRUE); \
						continue; \
					} \
						if(!g_ascii_strncasecmp(c, "<" x, strlen("<" x)) && \
								(*(c+strlen("<" x)) == '>' || \
								 !g_ascii_strncasecmp(c+strlen("<" x), "/>", 2))) { \
							xhtml = g_string_append(xhtml, "<" y); \
							c += strlen("<" x); \
							if(*c != '/') { \
								struct gaim_parse_tag *pt = g_new0(struct gaim_parse_tag, 1); \
								pt->src_tag = x; \
								pt->dest_tag = y; \
								tags = g_list_prepend(tags, pt); \
								xhtml = g_string_append_c(xhtml, '>'); \
							} else { \
								xhtml = g_string_append(xhtml, "/>");\
							} \
							c = strchr(c, '>') + 1; \
							continue; \
						}
#define ALLOW_TAG(x) ALLOW_TAG_ALT(x, x)
void
gaim_markup_html_to_xhtml(const char *html, char **xhtml_out,
						  char **plain_out)
{
	GString *xhtml = g_string_new("");
	GString *plain = g_string_new("");
	GList *tags = NULL, *tag;
	const char *c = html;

	while(c && *c) {
		if(*c == '<') {
			if(*(c+1) == '/') { /* closing tag */
				tag = tags;
				while(tag) {
					struct gaim_parse_tag *pt = tag->data;
					if(!g_ascii_strncasecmp((c+2), pt->src_tag, strlen(pt->src_tag)) && *(c+strlen(pt->src_tag)+2) == '>') {
						c += strlen(pt->src_tag) + 3;
						break;
					}
					tag = tag->next;
				}
				if(tag) {
					while(tags) {
						struct gaim_parse_tag *pt = tags->data;
						g_string_append_printf(xhtml, "</%s>", pt->dest_tag);
						if(tags == tag)
							break;
						tags = g_list_remove(tags, pt);
						g_free(pt);
					}
					g_free(tag->data);
					tags = g_list_remove(tags, tag->data);
				} else {
					/* a closing tag we weren't expecting...
					 * we'll let it slide, if it's really a tag...if it's
					 * just a </ we'll escape it properly */
					const char *end = c+2;
					while(*end && g_ascii_isalpha(*end))
						end++;
					if(*end == '>') {
						c = end+1;
					} else {
						xhtml = g_string_append(xhtml, "&lt;");
						plain = g_string_append_c(plain, '<');
						c++;
					}
				}
			} else { /* opening tag */
				ALLOW_TAG("a");
				ALLOW_TAG("blockquote");
				ALLOW_TAG("cite");
				ALLOW_TAG("div");
				ALLOW_TAG("em");
				ALLOW_TAG("h1");
				ALLOW_TAG("h2");
				ALLOW_TAG("h3");
				ALLOW_TAG("h4");
				ALLOW_TAG("h5");
				ALLOW_TAG("h6");
				/* we only allow html to start the message */
				if(c == html)
					ALLOW_TAG("html");
				ALLOW_TAG_ALT("i", "em");
				ALLOW_TAG_ALT("italic", "em");
				ALLOW_TAG("li");
				ALLOW_TAG("ol");
				ALLOW_TAG("p");
				ALLOW_TAG("pre");
				ALLOW_TAG("q");
				ALLOW_TAG("span");
				ALLOW_TAG("strong");
				ALLOW_TAG("ul");

				/* we skip <HR> because it's not legal in XHTML-IM.  However,
				 * we still want to send something sensible, so we put a
				 * linebreak in its place. <BR> also needs special handling
				 * because putting a </BR> to close it would just be dumb. */
				if((!g_ascii_strncasecmp(c, "<br", 3)
							|| !g_ascii_strncasecmp(c, "<hr", 3))
						&& (*(c+3) == '>' ||
							!g_ascii_strncasecmp(c+3, "/>", 2) ||
							!g_ascii_strncasecmp(c+3, " />", 3))) {
					c = strchr(c, '>') + 1;
					xhtml = g_string_append(xhtml, "<br/>");
					if(*c != '\n')
						plain = g_string_append_c(plain, '\n');
					continue;
				}
				if(!g_ascii_strncasecmp(c, "<b>", 3) || !g_ascii_strncasecmp(c, "<bold>", strlen("<bold>"))) {
					struct gaim_parse_tag *pt = g_new0(struct gaim_parse_tag, 1);
					pt->src_tag = *(c+2) == '>' ? "b" : "bold";
					pt->dest_tag = "span";
					tags = g_list_prepend(tags, pt);
					c = strchr(c, '>') + 1;
					xhtml = g_string_append(xhtml, "<span style='font-weight: bold;'>");
					continue;
				}
				if(!g_ascii_strncasecmp(c, "<u>", 3) || !g_ascii_strncasecmp(c, "<underline>", strlen("<underline>"))) {
					struct gaim_parse_tag *pt = g_new0(struct gaim_parse_tag, 1);
					pt->src_tag = *(c+2) == '>' ? "u" : "underline";
					pt->dest_tag = "span";
					tags = g_list_prepend(tags, pt);
					c = strchr(c, '>') + 1;
					xhtml = g_string_append(xhtml, "<span style='text-decoration: underline;'>");
					continue;
				}
				if(!g_ascii_strncasecmp(c, "<s>", 3) || !g_ascii_strncasecmp(c, "<strike>", strlen("<strike>"))) {
					struct gaim_parse_tag *pt = g_new0(struct gaim_parse_tag, 1);
					pt->src_tag = *(c+2) == '>' ? "s" : "strike";
					pt->dest_tag = "span";
					tags = g_list_prepend(tags, pt);
					c = strchr(c, '>') + 1;
					xhtml = g_string_append(xhtml, "<span style='text-decoration: line-through;'>");
					continue;
				}
				if(!g_ascii_strncasecmp(c, "<sub>", 5)) {
					struct gaim_parse_tag *pt = g_new0(struct gaim_parse_tag, 1);
					pt->src_tag = "sub";
					pt->dest_tag = "span";
					tags = g_list_prepend(tags, pt);
					c = strchr(c, '>') + 1;
					xhtml = g_string_append(xhtml, "<span style='vertical-align:sub;'>");
					continue;
				}
				if(!g_ascii_strncasecmp(c, "<sup>", 5)) {
					struct gaim_parse_tag *pt = g_new0(struct gaim_parse_tag, 1);
					pt->src_tag = "sup";
					pt->dest_tag = "span";
					tags = g_list_prepend(tags, pt);
					c = strchr(c, '>') + 1;
					xhtml = g_string_append(xhtml, "<span style='vertical-align:super;'>");
					continue;
				}
				if(!g_ascii_strncasecmp(c, "<font", 5) && (*(c+5) == '>' || *(c+5) == ' ')) {
					const char *p = c;
					GString *style = g_string_new("");
					struct gaim_parse_tag *pt;
					while(*p && *p != '>') {
						if(!g_ascii_strncasecmp(p, "back=", strlen("back="))) {
							const char *q = p + strlen("back=");
							GString *color = g_string_new("");
							if(*q == '\'' || *q == '\"')
								q++;
							while(*q && *q != '\"' && *q != '\'' && *q != ' ') {
								color = g_string_append_c(color, *q);
								q++;
							}
							g_string_append_printf(style, "background: %s; ", color->str);
							g_string_free(color, TRUE);
							p = q;
						} else if(!g_ascii_strncasecmp(p, "color=", strlen("color="))) {
							const char *q = p + strlen("color=");
							GString *color = g_string_new("");
							if(*q == '\'' || *q == '\"')
								q++;
							while(*q && *q != '\"' && *q != '\'' && *q != ' ') {
								color = g_string_append_c(color, *q);
								q++;
							}
							g_string_append_printf(style, "color: %s; ", color->str);
							g_string_free(color, TRUE);
							p = q;
						} else if(!g_ascii_strncasecmp(p, "face=", strlen("face="))) {
							const char *q = p + strlen("face=");
							gboolean space_allowed = FALSE;
							GString *face = g_string_new("");
							if(*q == '\'' || *q == '\"') {
								space_allowed = TRUE;
								q++;
							}
							while(*q && *q != '\"' && *q != '\'' && (space_allowed || *q != ' ')) {
								face = g_string_append_c(face, *q);
								q++;
							}
							g_string_append_printf(style, "font-family: %s; ", g_strstrip(face->str));
							g_string_free(face, TRUE);
							p = q;
						} else if(!g_ascii_strncasecmp(p, "size=", strlen("size="))) {
							const char *q = p + strlen("size=");
							int sz;
							const char *size = "medium";
							if(*q == '\'' || *q == '\"')
								q++;
							sz = atoi(q);
							switch (sz)
							{
							case 1:
							  size = "xx-small";
							  break;
							case 2:
							  size = "x-small";
							  break;
							case 3:
							  size = "small";
							  break;
							case 4:
							  size = "medium";
							  break;
							case 5:
							  size = "large";
							  break;
							case 6:
							  size = "x-large";
							  break;
							case 7:
							  size = "xx-large";
							  break;
							default:
							  break;
							}
							g_string_append_printf(style, "font-size: %s; ", size);
							p = q;
						}
						p++;
					}
					if ((c = strchr(c, '>')) != NULL)
						c++;
					else
						c = p;
					pt = g_new0(struct gaim_parse_tag, 1);
					pt->src_tag = "font";
					pt->dest_tag = "span";
					tags = g_list_prepend(tags, pt);
					if(style->len)
						g_string_append_printf(xhtml, "<span style='%s'>", g_strstrip(style->str));
					else
						pt->ignore = TRUE;
					g_string_free(style, TRUE);
					continue;
				}
				if(!g_ascii_strncasecmp(c, "<body ", 6)) {
					const char *p = c;
					gboolean did_something = FALSE;
					while(*p && *p != '>') {
						if(!g_ascii_strncasecmp(p, "bgcolor=", strlen("bgcolor="))) {
							const char *q = p + strlen("bgcolor=");
							struct gaim_parse_tag *pt = g_new0(struct gaim_parse_tag, 1);
							GString *color = g_string_new("");
							if(*q == '\'' || *q == '\"')
								q++;
							while(*q && *q != '\"' && *q != '\'' && *q != ' ') {
								color = g_string_append_c(color, *q);
								q++;
							}
							g_string_append_printf(xhtml, "<span style='background: %s;'>", g_strstrip(color->str));
							g_string_free(color, TRUE);
							if ((c = strchr(c, '>')) != NULL)
								c++;
							else
								c = p;
							pt->src_tag = "body";
							pt->dest_tag = "span";
							tags = g_list_prepend(tags, pt);
							did_something = TRUE;
							break;
						}
						p++;
					}
					if(did_something) continue;
				}
				/* this has to come after the special case for bgcolor */
				ALLOW_TAG("body");
				if(!g_ascii_strncasecmp(c, "<!--", strlen("<!--"))) {
					char *p = strstr(c + strlen("<!--"), "-->");
					if(p) {
						xhtml = g_string_append(xhtml, "<!--");
						c += strlen("<!--");
						continue;
					}
				}

				xhtml = g_string_append(xhtml, "&lt;");
				plain = g_string_append_c(plain, '<');
				c++;
			}
		} else if(*c == '&') {
			char buf[7];
			const char *pln;
			int len;

			if ((pln = detect_entity(c, &len)) == NULL) {
				len = 1;
				g_snprintf(buf, sizeof(buf), "%c", *c);
				pln = buf;
			}
			xhtml = g_string_append_len(xhtml, c, len);
			plain = g_string_append(plain, pln);
			c += len;
		} else {
			xhtml = g_string_append_c(xhtml, *c);
			plain = g_string_append_c(plain, *c);
			c++;
		}
	}
	tag = tags;
	while(tag) {
		struct gaim_parse_tag *pt = tag->data;
		if(!pt->ignore)
			g_string_append_printf(xhtml, "</%s>", pt->dest_tag);
		tag = tag->next;
	}
	g_list_free(tags);
	if(xhtml_out)
		*xhtml_out = g_strdup(xhtml->str);
	if(plain_out)
		*plain_out = g_strdup(plain->str);
	g_string_free(xhtml, TRUE);
	g_string_free(plain, TRUE);
}

/* The following are probably reasonable changes:
 * - \n should be converted to a normal space
 * - in addition to <br>, <p> and <div> etc. should also be converted into \n
 * - We want to turn </td>#whitespace<td> sequences into a single tab
 * - We want to turn <td> into a single tab (for msn profile "parsing")
 * - We want to turn </tr>#whitespace<tr> sequences into a single \n
 * - <script>...</script> and <style>...</style> should be completely removed
 */

char *
gaim_markup_strip_html(const char *str)
{
	int i, j, k, entlen;
	gboolean visible = TRUE;
	gboolean closing_td_p = FALSE;
	gchar *str2;
	const gchar *cdata_close_tag = NULL, *ent;
	gchar *href = NULL;
	int href_st = 0;

	if(!str)
		return NULL;

	str2 = g_strdup(str);

	for (i = 0, j = 0; str2[i]; i++)
	{
		if (str2[i] == '<')
		{
			if (cdata_close_tag)
			{
				/* Note: Don't even assume any other tag is a tag in CDATA */
				if (strncasecmp(str2 + i, cdata_close_tag,
						strlen(cdata_close_tag)) == 0)
				{
					i += strlen(cdata_close_tag) - 1;
					cdata_close_tag = NULL;
				}
				continue;
			}
			else if (strncasecmp(str2 + i, "<td", 3) == 0 && closing_td_p)
			{
				str2[j++] = '\t';
				visible = TRUE;
			}
			else if (strncasecmp(str2 + i, "</td>", 5) == 0)
			{
				closing_td_p = TRUE;
				visible = FALSE;
			}
			else
			{
				closing_td_p = FALSE;
				visible = TRUE;
			}

			k = i + 1;

			if(g_ascii_isspace(str2[k]))
				visible = TRUE;
			else if (str2[k])
			{
				/* Scan until we end the tag either implicitly (closed start
				 * tag) or explicitly, using a sloppy method (i.e., < or >
				 * inside quoted attributes will screw us up)
				 */
				while (str2[k] && str2[k] != '<' && str2[k] != '>')
				{
					k++;
				}

				/* If we've got an <a> tag with an href, save the address
				 * to print later. */
				if (strncasecmp(str2 + i, "<a", 2) == 0 &&
				    g_ascii_isspace(str2[i+2]))
				{
					int st; /* start of href, inclusive [ */
					int end; /* end of href, exclusive ) */
					char delim = ' ';
					/* Find start of href */
					for (st = i + 3; st < k; st++)
					{
						if (strncasecmp(str2+st, "href=", 5) == 0)
						{
							st += 5;
							if (str2[st] == '"')
							{
								delim = '"';
								st++;
							}
							break;
						}
					}
					/* find end of address */
					for (end = st; end < k && str2[end] != delim; end++)
					{
						/* All the work is done in the loop construct above. */
					}

					/* If there's an address, save it.  If there was
					 * already one saved, kill it. */
					if (st < k)
					{
						char *tmp;
						g_free(href);
						tmp = g_strndup(str2 + st, end - st);
						href = gaim_unescape_html(tmp);
						g_free(tmp);
						href_st = j;
					}
				}

				/* Replace </a> with an ascii representation of the
				 * address the link was pointing to. */
				else if (href != NULL && strncasecmp(str2 + i, "</a>", 4) == 0)
				{

					size_t hrlen = strlen(href);

					/* Only insert the href if it's different from the CDATA. */
					if ((hrlen != j - href_st ||
					     strncmp(str2 + href_st, href, hrlen)) &&
					    (hrlen != j - href_st + 7 || /* 7 == strlen("http://") */
					     strncmp(str2 + href_st, href + 7, hrlen - 7)))
					{
						str2[j++] = ' ';
						str2[j++] = '(';
						g_memmove(str2 + j, href, hrlen);
						j += hrlen;
						str2[j++] = ')';
						g_free(href);
						href = NULL;
					}
				}

				/* Check for tags which should be mapped to newline */
				else if (strncasecmp(str2 + i, "<p>", 3) == 0
				 || strncasecmp(str2 + i, "<tr", 3) == 0
				 || strncasecmp(str2 + i, "<br", 3) == 0
				 || strncasecmp(str2 + i, "<li", 3) == 0
				 || strncasecmp(str2 + i, "<div", 4) == 0
				 || strncasecmp(str2 + i, "</table>", 8) == 0)
				{
					str2[j++] = '\n';
				}
				/* Check for tags which begin CDATA and need to be closed */
#if 0 /* FIXME.. option is end tag optional, we can't handle this right now */
				else if (strncasecmp(str2 + i, "<option", 7) == 0)
				{
					/* FIXME: We should not do this if the OPTION is SELECT'd */
					cdata_close_tag = "</option>";
				}
#endif
				else if (strncasecmp(str2 + i, "<script", 7) == 0)
				{
					cdata_close_tag = "</script>";
				}
				else if (strncasecmp(str2 + i, "<style", 6) == 0)
				{
					cdata_close_tag = "</style>";
				}
				/* Update the index and continue checking after the tag */
				i = (str2[k] == '<' || str2[k] == '\0')? k - 1: k;
				continue;
			}
		}
		else if (cdata_close_tag)
		{
			continue;
		}
		else if (!g_ascii_isspace(str2[i]))
		{
			visible = TRUE;
		}

		if (str2[i] == '&' && (ent = detect_entity(str2 + i, &entlen)) != NULL)
		{
			while (*ent)
				str2[j++] = *ent++;
			i += entlen - 1;
			continue;
		}

		if (visible)
			str2[j++] = g_ascii_isspace(str2[i])? ' ': str2[i];
	}

	g_free(href);

	str2[j] = '\0';

	return str2;
}

static gboolean
badchar(char c)
{
	switch (c) {
	case ' ':
	case ',':
	case '\0':
	case '\n':
	case '\r':
	case '<':
	case '>':
	case '"':
	case '\'':
		return TRUE;
	default:
		return FALSE;
	}
}

static gboolean
badentity(const char *c)
{
	if (!g_ascii_strncasecmp(c, "&lt;", 4) ||
		!g_ascii_strncasecmp(c, "&gt;", 4) ||
		!g_ascii_strncasecmp(c, "&quot;", 6)) {
		return TRUE;
	}
	return FALSE;
}

char *
gaim_markup_linkify(const char *text)
{
	const char *c, *t, *q = NULL;
	char *tmpurlbuf, *url_buf;
	gunichar g;
	gboolean inside_html = FALSE;
	int inside_paren = 0;
	GString *ret = g_string_new("");
	/* Assumes you have a buffer able to carry at least BUF_LEN * 2 bytes */

	c = text;
	while (*c) {

		if(*c == '(' && !inside_html) {
			inside_paren++;
			ret = g_string_append_c(ret, *c);
			c++;
		}

		if(inside_html) {
			if(*c == '>') {
				inside_html = FALSE;
			} else if(!q && (*c == '\"' || *c == '\'')) {
				q = c;
			} else if(q) {
				if(*c == *q)
					q = NULL;
			}
		} else if(*c == '<') {
			inside_html = TRUE;
			if (!g_ascii_strncasecmp(c, "<A", 2)) {
				while (1) {
					if (!g_ascii_strncasecmp(c, "/A>", 3)) {
						inside_html = FALSE;
						break;
					}
					ret = g_string_append_c(ret, *c);
					c++;
					if (!(*c))
						break;
				}
			}
		} else if ((*c=='h') && (!g_ascii_strncasecmp(c, "http://", 7) ||
					(!g_ascii_strncasecmp(c, "https://", 8)))) {
			t = c;
			while (1) {
				if (badchar(*t) || badentity(t)) {

					if (*(t) == ',' && (*(t + 1) != ' ')) {
						t++;
						continue;
					}

					if (*(t - 1) == '.')
						t--;
					if ((*(t - 1) == ')' && (inside_paren > 0))) {
						t--;
					}

					url_buf = g_strndup(c, t - c);
					tmpurlbuf = gaim_unescape_html(url_buf);
					g_string_append_printf(ret, "<A HREF=\"%s\">%s</A>",
							tmpurlbuf, url_buf);
					g_free(url_buf);
					g_free(tmpurlbuf);
					c = t;
					break;
				}
				t++;

			}
		} else if (!g_ascii_strncasecmp(c, "www.", 4) && (c == text || badchar(c[-1]) || badentity(c-1))) {
			if (c[4] != '.') {
				t = c;
				while (1) {
					if (badchar(*t) || badentity(t)) {
						if (t - c == 4) {
							break;
						}

						if (*(t) == ',' && (*(t + 1) != ' ')) {
							t++;
							continue;
						}

						if (*(t - 1) == '.')
							t--;
						if ((*(t - 1) == ')' && (inside_paren > 0))) {
							t--;
						}
						url_buf = g_strndup(c, t - c);
						tmpurlbuf = gaim_unescape_html(url_buf);
						g_string_append_printf(ret,
								"<A HREF=\"http://%s\">%s</A>", tmpurlbuf,
								url_buf);
						g_free(url_buf);
						g_free(tmpurlbuf);
						c = t;
						break;
					}
					t++;
				}
			}
		} else if (!g_ascii_strncasecmp(c, "ftp://", 6) || !g_ascii_strncasecmp(c, "sftp://", 7)) {
			t = c;
			while (1) {
				if (badchar(*t) || badentity(t)) {
					if (*(t - 1) == '.')
						t--;
					if ((*(t - 1) == ')' && (inside_paren > 0))) {
						t--;
					}
					url_buf = g_strndup(c, t - c);
					tmpurlbuf = gaim_unescape_html(url_buf);
					g_string_append_printf(ret, "<A HREF=\"%s\">%s</A>",
							tmpurlbuf, url_buf);
					g_free(url_buf);
					g_free(tmpurlbuf);
					c = t;
					break;
				}
				if (!t)
					break;
				t++;

			}
		} else if (!g_ascii_strncasecmp(c, "ftp.", 4) && (c == text || badchar(c[-1]) || badentity(c-1))) {
			if (c[4] != '.') {
				t = c;
				while (1) {
					if (badchar(*t) || badentity(t)) {
						if (t - c == 4) {
							break;
						}
						if (*(t - 1) == '.')
							t--;
						if ((*(t - 1) == ')' && (inside_paren > 0))) {
							t--;
						}
						url_buf = g_strndup(c, t - c);
						tmpurlbuf = gaim_unescape_html(url_buf);
						g_string_append_printf(ret,
								"<A HREF=\"ftp://%s\">%s</A>", tmpurlbuf,
								url_buf);
						g_free(url_buf);
						g_free(tmpurlbuf);
						c = t;
						break;
					}
					if (!t)
						break;
					t++;
				}
			}
		} else if (!g_ascii_strncasecmp(c, "mailto:", 7)) {
			t = c;
			while (1) {
				if (badchar(*t) || badentity(t)) {
					if (*(t - 1) == '.')
						t--;
					url_buf = g_strndup(c, t - c);
					tmpurlbuf = gaim_unescape_html(url_buf);
					g_string_append_printf(ret, "<A HREF=\"%s\">%s</A>",
							  tmpurlbuf, url_buf);
					g_free(url_buf);
					g_free(tmpurlbuf);
					c = t;
					break;
				}
				if (!t)
					break;
				t++;

			}
		} else if (c != text && (*c == '@')) {
			int flag;
			GString *gurl_buf = NULL;
			const char illegal_chars[] = "!@#$%^&*()[]{}/|\\<>\":;\r\n \0";

			if (strchr(illegal_chars,*(c - 1)) || strchr(illegal_chars, *(c + 1)))
				flag = 0;
			else {
				flag = 1;
				gurl_buf = g_string_new("");
			}

			t = c;
			while (flag) {
				/* iterate backwards grabbing the local part of an email address */
				g = g_utf8_get_char(t);
				if (badchar(*t) || (g >= 127) || (*t == '(') ||
					((*t == ';') && ((t > (text+2) && (!g_ascii_strncasecmp(t - 3, "&lt;", 4) ||
				                                       !g_ascii_strncasecmp(t - 3, "&gt;", 4))) ||
				                     (t > (text+4) && (!g_ascii_strncasecmp(t - 5, "&quot;", 6)))))) {
					/* local part will already be part of ret, strip it out */
					ret = g_string_truncate(ret, ret->len - (c - t));
					ret = g_string_append_unichar(ret, g);
					break;
				} else {
					g_string_prepend_unichar(gurl_buf, g);
					t = g_utf8_find_prev_char(text, t);
					if (t < text) {
						ret = g_string_assign(ret, "");
						break;
					}
				}
			}

			t = g_utf8_find_next_char(c, NULL);

			while (flag) {
				/* iterate forwards grabbing the domain part of an email address */
				g = g_utf8_get_char(t);
				if (badchar(*t) || (g >= 127) || (*t == ')') || badentity(t)) {
					char *d;

					url_buf = g_string_free(gurl_buf, FALSE);

					/* strip off trailing periods */
					if (strlen(url_buf) > 0) {
						for (d = url_buf + strlen(url_buf) - 1; *d == '.'; d--, t--)
							*d = '\0';
					}

					tmpurlbuf = gaim_unescape_html(url_buf);
					if (gaim_email_is_valid(tmpurlbuf)) {
						g_string_append_printf(ret, "<A HREF=\"mailto:%s\">%s</A>",
								tmpurlbuf, url_buf);
					} else {
						g_string_append(ret, url_buf);
					}
					g_free(url_buf);
					g_free(tmpurlbuf);
					c = t;

					break;
				} else {
					g_string_append_unichar(gurl_buf, g);
					t = g_utf8_find_next_char(t, NULL);
				}
			}
		}

		if(*c == ')' && !inside_html) {
			inside_paren--;
			ret = g_string_append_c(ret, *c);
			c++;
		}

		if (*c == 0)
			break;

		ret = g_string_append_c(ret, *c);
		c++;

	}
	return g_string_free(ret, FALSE);
}

char *
gaim_unescape_html(const char *html) {
	if (html != NULL) {
		const char *c = html;
		GString *ret = g_string_new("");
		while (*c) {
			int len;
			const char *ent;

			if ((ent = detect_entity(c, &len)) != NULL) {
				ret = g_string_append(ret, ent);
				c += len;
			} else if (!strncmp(c, "<br>", 4)) {
				ret = g_string_append_c(ret, '\n');
				c += 4;
			} else {
				ret = g_string_append_c(ret, *c);
				c++;
			}
		}
		return g_string_free(ret, FALSE);
	}

	return NULL;
}

char *
gaim_markup_slice(const char *str, guint x, guint y)
{
	GString *ret;
	GQueue *q;
	guint z = 0;
	gboolean appended = FALSE;
	gunichar c;
	char *tag;

	g_return_val_if_fail(x <= y, NULL);

	if (x == y)
		return g_strdup("");

	ret = g_string_new("");
	q = g_queue_new();

	while (*str && (z < y)) {
		c = g_utf8_get_char(str);

		if (c == '<') {
			char *end = strchr(str, '>');

			if (!end) {
				g_string_free(ret, TRUE);
				while ((tag = g_queue_pop_head(q)))
					g_free(tag);
				g_queue_free(q);
				return NULL;
			}

			if (!g_ascii_strncasecmp(str, "<img ", 5)) {
				z += strlen("[Image]");
			} else if (!g_ascii_strncasecmp(str, "<br", 3)) {
				z += 1;
			} else if (!g_ascii_strncasecmp(str, "<hr>", 4)) {
				z += strlen("\n---\n");
			} else if (!g_ascii_strncasecmp(str, "</", 2)) {
				/* pop stack */
				char *tmp;

				tmp = g_queue_pop_head(q);
				g_free(tmp);
				/* z += 0; */
			} else {
				/* push it unto the stack */
				char *tmp;

				tmp = g_strndup(str, end - str + 1);
				g_queue_push_head(q, tmp);
				/* z += 0; */
			}

			if (z >= x) {
				g_string_append_len(ret, str, end - str + 1);
			}

			str = end;
		} else if (c == '&') {
			char *end = strchr(str, ';');
			if (!end) {
				g_string_free(ret, TRUE);
				while ((tag = g_queue_pop_head(q)))
					g_free(tag);
				g_queue_free(q);

				return NULL;
			}

			if (z >= x)
				g_string_append_len(ret, str, end - str + 1);

			z++;
			str = end;
		} else {
			if (z == x && z > 0 && !appended) {
				GList *l = q->tail;

				while (l) {
					tag = l->data;
					g_string_append(ret, tag);
					l = l->prev;
				}
				appended = TRUE;
			}

			if (z >= x)
				g_string_append_unichar(ret, c);
			z++;
		}

		str = g_utf8_next_char(str);
	}

	while ((tag = g_queue_pop_head(q))) {
		char *name;

		name = gaim_markup_get_tag_name(tag);
		g_string_append_printf(ret, "</%s>", name);
		g_free(name);
		g_free(tag);
	}

	g_queue_free(q);
	return g_string_free(ret, FALSE);
}

char *
gaim_markup_get_tag_name(const char *tag)
{
	int i;
	g_return_val_if_fail(tag != NULL, NULL);
	g_return_val_if_fail(*tag == '<', NULL);

	for (i = 1; tag[i]; i++)
		if (tag[i] == '>' || tag[i] == ' ' || tag[i] == '/')
			break;

	return g_strndup(tag+1, i-1);
}

/**************************************************************************
 * Path/Filename Functions
 **************************************************************************/
const char *
gaim_home_dir(void)
{
#ifndef _WIN32
	return g_get_home_dir();
#else
	return wgaim_data_dir();
#endif
}

/* returns a string of the form ~/.gaim, where ~ is replaced by the user's home
 * dir. Note that there is no trailing slash after .gaim. */
const char *
gaim_user_dir(void)
{
	if (custom_home_dir != NULL && strlen(custom_home_dir) > 0) {
		strcpy ((char*) &home_dir, (char*) &custom_home_dir);
	} else {
		const gchar *hd = gaim_home_dir();

		if (hd) {
			g_strlcpy((char*) &home_dir, hd, sizeof(home_dir));
			g_strlcat((char*) &home_dir, G_DIR_SEPARATOR_S ".gaim",
					sizeof(home_dir));
		}
	}

	return home_dir;
}

void gaim_util_set_user_dir(const char *dir)
{
	if (dir != NULL && strlen(dir) > 0) {
		g_strlcpy((char*) &custom_home_dir, dir,
				sizeof(custom_home_dir));
	}
}

int gaim_build_dir (const char *path, int mode)
{
#if GLIB_CHECK_VERSION(2,8,0)
	return g_mkdir_with_parents(path, mode);
#else
	char *dir, **components, delim[] = { G_DIR_SEPARATOR, '\0' };
	int cur, len;

	g_return_val_if_fail(path != NULL, -1);

	dir = g_new0(char, strlen(path) + 1);
	components = g_strsplit(path, delim, -1);
	len = 0;
	for (cur = 0; components[cur] != NULL; cur++) {
		/* If you don't know what you're doing on both
		 * win32 and *NIX, stay the hell away from this code */
		if(cur > 1)
			dir[len++] = G_DIR_SEPARATOR;
		strcpy(dir + len, components[cur]);
		len += strlen(components[cur]);
		if(cur == 0)
			dir[len++] = G_DIR_SEPARATOR;

		if(g_file_test(dir, G_FILE_TEST_IS_DIR)) {
			continue;
#ifdef _WIN32
		/* allow us to create subdirs on UNC paths
		 * (\\machinename\path\to\blah)
		 * g_file_test() doesn't work on "\\machinename" */
		} else if (cur == 2 && dir[0] == '\\' && dir[1] == '\\'
				&& components[cur + 1] != NULL) {
			continue;
#endif
		} else if(g_file_test(dir, G_FILE_TEST_EXISTS)) {
			gaim_debug_warning("build_dir", "bad path: %s\n", path);
			g_strfreev(components);
			g_free(dir);
			return -1;
		}

		if (g_mkdir(dir, mode) < 0) {
			gaim_debug_warning("build_dir", "mkdir: %s\n", strerror(errno));
			g_strfreev(components);
			g_free(dir);
			return -1;
		}
	}

	g_strfreev(components);
	g_free(dir);
	return 0;
#endif
}

/*
 * This function is long and beautiful, like my--um, yeah.  Anyway,
 * it includes lots of error checking so as we don't overwrite
 * people's settings if there is a problem writing the new values.
 */
gboolean
gaim_util_write_data_to_file(const char *filename, const char *data, size_t size)
{
	const char *user_dir = gaim_user_dir();
	gchar *filename_temp, *filename_full;
	FILE *file;
	size_t real_size, byteswritten;
	struct stat st;

	g_return_val_if_fail(user_dir != NULL, FALSE);

	gaim_debug_info("util", "Writing file %s to directory %s\n",
					filename, user_dir);

	/* Ensure the user directory exists */
	if (!g_file_test(user_dir, G_FILE_TEST_IS_DIR))
	{
		if (g_mkdir(user_dir, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
		{
			gaim_debug_error("util", "Error creating directory %s: %s\n",
							 user_dir, strerror(errno));
			return FALSE;
		}
	}

	filename_full = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", user_dir, filename);
	filename_temp = g_strdup_printf("%s.save", filename_full);

	/* Remove an old temporary file, if one exists */
	if (g_file_test(filename_temp, G_FILE_TEST_EXISTS))
	{
		if (g_unlink(filename_temp) == -1)
		{
			gaim_debug_error("util", "Error removing old file %s: %s\n",
							 filename_temp, strerror(errno));
		}
	}

	/* Open file */
	file = g_fopen(filename_temp, "wb");
	if (file == NULL)
	{
		gaim_debug_error("util", "Error opening file %s for writing: %s\n",
						 filename_temp, strerror(errno));
		g_free(filename_full);
		g_free(filename_temp);
		return FALSE;
	}

	/* Write to file */
	real_size = (size == -1) ? strlen(data) : size;
	byteswritten = fwrite(data, 1, real_size, file);

	/* Close file */
	if (fclose(file) != 0)
	{
		gaim_debug_error("util", "Error closing file %s: %s\n",
						 filename_temp, strerror(errno));
		g_free(filename_full);
		g_free(filename_temp);
		return FALSE;
	}

	/* Ensure the file is the correct size */
	if (byteswritten != real_size)
	{
		gaim_debug_error("util", "Error writing to file %s: Wrote %" G_GSIZE_FORMAT " bytes "
						 "but should have written %" G_GSIZE_FORMAT "; is your disk full?\n",
						 filename_temp, byteswritten, real_size);
		g_free(filename_full);
		g_free(filename_temp);
		return FALSE;
	}
	/* Use stat to be absolutely sure. */
	if ((g_stat(filename_temp, &st) == -1) || (st.st_size != real_size))
	{
		gaim_debug_error("util", "Error writing data to file %s: "
						 "Incomplete file written; is your disk full?\n",
						 filename_temp);
		g_free(filename_full);
		g_free(filename_temp);
		return FALSE;
	}

#ifndef _WIN32
	/* Set file permissions */
	if (chmod(filename_temp, S_IRUSR | S_IWUSR) == -1)
	{
		gaim_debug_error("util", "Error setting permissions of file %s: %s\n",
						 filename_temp, strerror(errno));
	}
#endif

	/* Rename to the REAL name */
	if (g_rename(filename_temp, filename_full) == -1)
	{
		gaim_debug_error("util", "Error renaming %s to %s: %s\n",
						 filename_temp, filename_full, strerror(errno));
	}

	g_free(filename_full);
	g_free(filename_temp);

	return TRUE;
}

xmlnode *
gaim_util_read_xml_from_file(const char *filename, const char *description)
{
	const char *user_dir = gaim_user_dir();
	gchar *filename_full;
	GError *error = NULL;
	gchar *contents = NULL;
	gsize length;
	xmlnode *node = NULL;

	g_return_val_if_fail(user_dir != NULL, NULL);

	gaim_debug_info("util", "Reading file %s from directory %s\n",
					filename, user_dir);

	filename_full = g_build_filename(user_dir, filename, NULL);

	if (!g_file_test(filename_full, G_FILE_TEST_EXISTS))
	{
		gaim_debug_info("util", "File %s does not exist (this is not "
						"necessarily an error)\n", filename_full);
		g_free(filename_full);
		return NULL;
	}

	if (!g_file_get_contents(filename_full, &contents, &length, &error))
	{
		gaim_debug_error("util", "Error reading file %s: %s\n",
						 filename_full, error->message);
		g_error_free(error);
	}

	if ((contents != NULL) && (length > 0))
	{
		node = xmlnode_from_str(contents, length);

		/* If we were unable to parse the file then save its contents to a backup file */
		if (node == NULL)
		{
			gchar *filename_temp;

			filename_temp = g_strdup_printf("%s~", filename);
			gaim_debug_error("util", "Error parsing file %s.  Renaming old "
							 "file to %s\n", filename_full, filename_temp);
			gaim_util_write_data_to_file(filename_temp, contents, length);
			g_free(filename_temp);
		}

		g_free(contents);
	}

	/* If we could not parse the file then show the user an error message */
	if (node == NULL)
	{
		gchar *title, *msg;
		title = g_strdup_printf(_("Error Reading %s"), filename);
		msg = g_strdup_printf(_("An error was encountered reading your "
					"%s.  They have not been loaded, and the old file "
					"has been renamed to %s~."), description, filename_full);
		gaim_notify_error(NULL, NULL, title, msg);
		g_free(title);
		g_free(msg);
	}

	g_free(filename_full);

	return node;
}

/*
 * Like mkstemp() but returns a file pointer, uses a pre-set template,
 * uses the semantics of tempnam() for the directory to use and allocates
 * the space for the filepath.
 *
 * Caller is responsible for closing the file and removing it when done,
 * as well as freeing the space pointed-to by "path" with g_free().
 *
 * Returns NULL on failure and cleans up after itself if so.
 */
static const char *gaim_mkstemp_templ = {"gaimXXXXXX"};

FILE *
gaim_mkstemp(char **fpath, gboolean binary)
{
	const gchar *tmpdir;
#ifndef _WIN32
	int fd;
#endif
	FILE *fp = NULL;

	g_return_val_if_fail(fpath != NULL, NULL);

	if((tmpdir = (gchar*)g_get_tmp_dir()) != NULL) {
		if((*fpath = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", tmpdir, gaim_mkstemp_templ)) != NULL) {
#ifdef _WIN32
			char* result = _mktemp( *fpath );
			if( result == NULL )
				gaim_debug(GAIM_DEBUG_ERROR, "gaim_mkstemp",
						   "Problem creating the template\n");
			else
			{
				if( (fp = g_fopen( result, binary?"wb+":"w+")) == NULL ) {
					gaim_debug(GAIM_DEBUG_ERROR, "gaim_mkstemp",
							   "Couldn't fopen() %s\n", result);
				}
			}
#else
			if((fd = mkstemp(*fpath)) == -1) {
				gaim_debug(GAIM_DEBUG_ERROR, "gaim_mkstemp",
						   "Couldn't make \"%s\", error: %d\n",
						   *fpath, errno);
			} else {
				if((fp = fdopen(fd, "r+")) == NULL) {
					close(fd);
					gaim_debug(GAIM_DEBUG_ERROR, "gaim_mkstemp",
							   "Couldn't fdopen(), error: %d\n", errno);
				}
			}
#endif
			if(!fp) {
				g_free(*fpath);
				*fpath = NULL;
			}
		}
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "gaim_mkstemp",
				   "g_get_tmp_dir() failed!\n");
	}

	return fp;
}

gboolean
gaim_program_is_valid(const char *program)
{
	GError *error = NULL;
	char **argv;
	gchar *progname;
	gboolean is_valid = FALSE;

	g_return_val_if_fail(program != NULL,  FALSE);
	g_return_val_if_fail(*program != '\0', FALSE);

	if (!g_shell_parse_argv(program, NULL, &argv, &error)) {
		gaim_debug(GAIM_DEBUG_ERROR, "program_is_valid",
				   "Could not parse program '%s': %s\n",
				   program, error->message);
		g_error_free(error);
		return FALSE;
	}

	if (argv == NULL) {
		return FALSE;
	}

	progname = g_find_program_in_path(argv[0]);
	is_valid = (progname != NULL);

	g_strfreev(argv);
	g_free(progname);

	return is_valid;
}


gboolean
gaim_running_gnome(void)
{
#ifndef _WIN32
	gchar *tmp = g_find_program_in_path("gnome-open");

	if (tmp == NULL)
		return FALSE;
	g_free(tmp);

	return (g_getenv("GNOME_DESKTOP_SESSION_ID") != NULL);
#else
	return FALSE;
#endif
}

gboolean
gaim_running_kde(void)
{
#ifndef _WIN32
	gchar *tmp = g_find_program_in_path("kfmclient");
	const char *session;

	if (tmp == NULL)
		return FALSE;
	g_free(tmp);

	session = g_getenv("KDE_FULL_SESSION");
	if (session != NULL && !strcmp(session, "true"))
		return TRUE;

	/* If you run Gaim from Konsole under !KDE, this will provide a
	 * a false positive.  Since we do the GNOME checks first, this is
	 * only a problem if you're running something !(KDE || GNOME) and
	 * you run Gaim from Konsole. This really shouldn't be a problem. */
	return ((g_getenv("KDEDIR") != NULL) || g_getenv("KDEDIRS") != NULL);
#else
	return FALSE;
#endif
}

gboolean
gaim_running_osx(void)
{
#if defined(__APPLE__)	
	return TRUE;
#else
	return FALSE;
#endif
}

char *
gaim_fd_get_ip(int fd)
{
	struct sockaddr addr;
	socklen_t namelen = sizeof(addr);

	g_return_val_if_fail(fd != 0, NULL);

	if (getsockname(fd, &addr, &namelen))
		return NULL;

	return g_strdup(inet_ntoa(((struct sockaddr_in *)&addr)->sin_addr));
}


/**************************************************************************
 * String Functions
 **************************************************************************/
const char *
gaim_normalize(const GaimAccount *account, const char *str)
{
	const char *ret = NULL;

	if (account != NULL)
	{
		GaimPlugin *prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));

		if (prpl != NULL)
		{
			GaimPluginProtocolInfo *prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

			if(prpl_info && prpl_info->normalize)
				ret = prpl_info->normalize(account, str);
		}
	}

	if (ret == NULL)
	{
		static char buf[BUF_LEN];
		char *tmp;

		tmp = g_utf8_normalize(str, -1, G_NORMALIZE_DEFAULT);
		g_snprintf(buf, sizeof(buf), "%s", tmp);
		g_free(tmp);

		ret = buf;
	}

	return ret;
}

/*
 * You probably don't want to call this directly, it is
 * mainly for use as a PRPL callback function.  See the
 * comments in util.h.
 */
const char *
gaim_normalize_nocase(const GaimAccount *account, const char *str)
{
	static char buf[BUF_LEN];
	char *tmp1, *tmp2;

	g_return_val_if_fail(str != NULL, NULL);

	tmp1 = g_utf8_strdown(str, -1);
	tmp2 = g_utf8_normalize(tmp1, -1, G_NORMALIZE_DEFAULT);
	g_snprintf(buf, sizeof(buf), "%s", tmp2 ? tmp2 : "");
	g_free(tmp2);
	g_free(tmp1);

	return buf;
}

gchar *
gaim_strdup_withhtml(const gchar *src)
{
	gulong destsize, i, j;
	gchar *dest;

	g_return_val_if_fail(src != NULL, NULL);

	/* New length is (length of src) + (number of \n's * 3) - (number of \r's) + 1 */
	destsize = 1;
	for (i = 0; src[i] != '\0'; i++)
	{
		if (src[i] == '\n')
			destsize += 4;
		else if (src[i] != '\r')
			destsize++;
	}

	dest = g_malloc(destsize);

	/* Copy stuff, ignoring \r's, because they are dumb */
	for (i = 0, j = 0; src[i] != '\0'; i++) {
		if (src[i] == '\n') {
			strcpy(&dest[j], "<BR>");
			j += 4;
		} else if (src[i] != '\r')
			dest[j++] = src[i];
	}

	dest[destsize-1] = '\0';

	return dest;
}

gboolean
gaim_str_has_prefix(const char *s, const char *p)
{
#if GLIB_CHECK_VERSION(2,2,0)
	return g_str_has_prefix(s, p);
#else
	g_return_val_if_fail(s != NULL, FALSE);
	g_return_val_if_fail(p != NULL, FALSE);

	return (!strncmp(s, p, strlen(p)));
#endif
}

gboolean
gaim_str_has_suffix(const char *s, const char *x)
{
#if GLIB_CHECK_VERSION(2,2,0)
	return g_str_has_suffix(s, x);
#else
	int off;

	g_return_val_if_fail(s != NULL, FALSE);
	g_return_val_if_fail(x != NULL, FALSE);

	off = strlen(s) - strlen(x);
	return (off >= 0 && !strcmp(s + off, x));
#endif
}

char *
gaim_str_add_cr(const char *text)
{
	char *ret = NULL;
	int count = 0, j;
	guint i;

	g_return_val_if_fail(text != NULL, NULL);

	if (text[0] == '\n')
		count++;
	for (i = 1; i < strlen(text); i++)
		if (text[i] == '\n' && text[i - 1] != '\r')
			count++;

	if (count == 0)
		return g_strdup(text);

	ret = g_malloc0(strlen(text) + count + 1);

	i = 0; j = 0;
	if (text[i] == '\n')
		ret[j++] = '\r';
	ret[j++] = text[i++];
	for (; i < strlen(text); i++) {
		if (text[i] == '\n' && text[i - 1] != '\r')
			ret[j++] = '\r';
		ret[j++] = text[i];
	}

	gaim_debug_misc("gaim_str_add_cr", "got: %s, leaving with %s\n",
					text, ret);

	return ret;
}

void
gaim_str_strip_char(char *text, char thechar)
{
	int i, j;

	g_return_if_fail(text != NULL);

	for (i = 0, j = 0; text[i]; i++)
		if (text[i] != thechar)
			text[j++] = text[i];

	text[j++] = '\0';
}

void
gaim_util_chrreplace(char *string, char delimiter,
					 char replacement)
{
	int i = 0;

	g_return_if_fail(string != NULL);

	while (string[i] != '\0')
	{
		if (string[i] == delimiter)
			string[i] = replacement;
		i++;
	}
}

gchar *
gaim_strreplace(const char *string, const char *delimiter,
				const char *replacement)
{
	gchar **split;
	gchar *ret;

	g_return_val_if_fail(string      != NULL, NULL);
	g_return_val_if_fail(delimiter   != NULL, NULL);
	g_return_val_if_fail(replacement != NULL, NULL);

	split = g_strsplit(string, delimiter, 0);
	ret = g_strjoinv(replacement, split);
	g_strfreev(split);

	return ret;
}

gchar *
gaim_strcasereplace(const char *string, const char *delimiter,
					const char *replacement)
{
	gchar *ret;
	int length_del, length_rep, i, j;

	g_return_val_if_fail(string      != NULL, NULL);
	g_return_val_if_fail(delimiter   != NULL, NULL);
	g_return_val_if_fail(replacement != NULL, NULL);

	length_del = strlen(delimiter);
	length_rep = strlen(replacement);

	/* Count how many times the delimiter appears */
	i = 0; /* position in the source string */
	j = 0; /* number of occurrences of "delimiter" */
	while (string[i] != '\0') {
		if (!strncasecmp(&string[i], delimiter, length_del)) {
			i += length_del;
			j += length_rep;
		} else {
			i++;
			j++;
		}
	}

	ret = g_malloc(j+1);

	i = 0; /* position in the source string */
	j = 0; /* position in the destination string */
	while (string[i] != '\0') {
		if (!strncasecmp(&string[i], delimiter, length_del)) {
			strncpy(&ret[j], replacement, length_rep);
			i += length_del;
			j += length_rep;
		} else {
			ret[j] = string[i];
			i++;
			j++;
		}
	}

	ret[j] = '\0';

	return ret;
}

const char *
gaim_strcasestr(const char *haystack, const char *needle)
{
	size_t hlen, nlen;
	const char *tmp, *ret;

	g_return_val_if_fail(haystack != NULL, NULL);
	g_return_val_if_fail(needle != NULL, NULL);

	hlen = strlen(haystack);
	nlen = strlen(needle);
	tmp = haystack,
	ret = NULL;

	g_return_val_if_fail(hlen > 0, NULL);
	g_return_val_if_fail(nlen > 0, NULL);

	while (*tmp && !ret) {
		if (!g_ascii_strncasecmp(needle, tmp, nlen))
			ret = tmp;
		else
			tmp++;
	}

	return ret;
}

char *
gaim_str_size_to_units(size_t size)
{
	static const char *size_str[4] = { "bytes", "KB", "MB", "GB" };
	float size_mag;
	int size_index = 0;

	if (size == -1) {
		return g_strdup(_("Calculating..."));
	}
	else if (size == 0) {
		return g_strdup(_("Unknown."));
	}
	else {
		size_mag = (float)size;

		while ((size_index < 3) && (size_mag > 1024)) {
			size_mag /= 1024;
			size_index++;
		}

		if (size_index == 0) {
			return g_strdup_printf("%" G_GSIZE_FORMAT " %s", size, size_str[size_index]);
		} else {
			return g_strdup_printf("%.2f %s", size_mag, size_str[size_index]);
		}
	}
}

char *
gaim_str_seconds_to_string(guint secs)
{
	char *ret = NULL;
	guint days, hrs, mins;

	if (secs < 60)
	{
		return g_strdup_printf(ngettext("%d second", "%d seconds", secs), secs);
	}

	days = secs / (60 * 60 * 24);
	secs = secs % (60 * 60 * 24);
	hrs  = secs / (60 * 60);
	secs = secs % (60 * 60);
	mins = secs / 60;
	secs = secs % 60;

	if (days > 0)
	{
		ret = g_strdup_printf(ngettext("%d day", "%d days", days), days);
	}

	if (hrs > 0)
	{
		if (ret != NULL)
		{
			char *tmp = g_strdup_printf(
					ngettext("%s, %d hour", "%s, %d hours", hrs),
							ret, hrs);
			g_free(ret);
			ret = tmp;
		}
		else
			ret = g_strdup_printf(ngettext("%d hour", "%d hours", hrs), hrs);
	}

	if (mins > 0)
	{
		if (ret != NULL)
		{
			char *tmp = g_strdup_printf(
					ngettext("%s, %d minute", "%s, %d minutes", mins),
							ret, mins);
			g_free(ret);
			ret = tmp;
		}
		else
			ret = g_strdup_printf(ngettext("%d minute", "%d minutes", mins), mins);
	}

	return ret;
}


char *
gaim_str_binary_to_ascii(const unsigned char *binary, guint len)
{
	GString *ret;
	guint i;

	g_return_val_if_fail(len > 0, NULL);

	ret = g_string_sized_new(len);

	for (i = 0; i < len; i++)
		if (binary[i] < 32 || binary[i] > 126)
			g_string_append_printf(ret, "\\x%02hhx", binary[i]);
		else if (binary[i] == '\\')
			g_string_append(ret, "\\\\");
		else
			g_string_append_c(ret, binary[i]);

	return g_string_free(ret, FALSE);
}

/**************************************************************************
 * URI/URL Functions
 **************************************************************************/
gboolean
gaim_url_parse(const char *url, char **ret_host, int *ret_port,
			   char **ret_path, char **ret_user, char **ret_passwd)
{
	char scan_info[255];
	char port_str[6];
	int f;
	const char *at, *slash;
	const char *turl;
	char host[256], path[256], user[256], passwd[256];
	int port = 0;
	/* hyphen at end includes it in control set */
	static char addr_ctrl[] = "A-Za-z0-9.-";
	static char port_ctrl[] = "0-9";
	static char page_ctrl[] = "A-Za-z0-9.~_/:*!@&%%?=+^-";
	static char user_ctrl[] = "A-Za-z0-9.~_/*!&%%?=+^-";
	static char passwd_ctrl[] = "A-Za-z0-9.~_/*!&%%?=+^-";

	g_return_val_if_fail(url != NULL, FALSE);

	if ((turl = strstr(url, "http://")) != NULL ||
		(turl = strstr(url, "HTTP://")) != NULL)
	{
		turl += 7;
		url = turl;
	}

	/* parse out authentication information if supplied */
	/* Only care about @ char BEFORE the first / */
	at = strchr(url, '@');
	slash = strchr(url, '/');
	if ((at != NULL) &&
			(((slash != NULL) && (strlen(at) > strlen(slash))) ||
			(slash == NULL))) {
		g_snprintf(scan_info, sizeof(scan_info),
					"%%255[%s]:%%255[%s]^@", user_ctrl, passwd_ctrl);
		f = sscanf(url, scan_info, user, passwd);

		if (f ==1 ) {
			/* No passwd, possibly just username supplied */
			g_snprintf(scan_info, sizeof(scan_info),
						"%%255[%s]^@", user_ctrl);
			f = sscanf(url, scan_info, user);
			*passwd = '\0';
		}

		url = at+1; /* move pointer after the @ char */
	} else {
		*user = '\0';
		*passwd = '\0';
	}

	g_snprintf(scan_info, sizeof(scan_info),
			   "%%255[%s]:%%5[%s]/%%255[%s]", addr_ctrl, port_ctrl, page_ctrl);

	f = sscanf(url, scan_info, host, port_str, path);

	if (f == 1)
	{
		g_snprintf(scan_info, sizeof(scan_info),
				   "%%255[%s]/%%255[%s]",
				   addr_ctrl, page_ctrl);
		f = sscanf(url, scan_info, host, path);
		g_snprintf(port_str, sizeof(port_str), "80");
	}

	if (f == 1)
		*path = '\0';

	sscanf(port_str, "%d", &port);

	if (ret_host != NULL) *ret_host = g_strdup(host);
	if (ret_port != NULL) *ret_port = port;
	if (ret_path != NULL) *ret_path = g_strdup(path);
	if (ret_user != NULL) *ret_user = g_strdup(user);
	if (ret_passwd != NULL) *ret_passwd = g_strdup(passwd);

	return TRUE;
}

/**
 * The arguments to this function are similar to printf.
 */
static void
gaim_util_fetch_url_error(GaimUtilFetchUrlData *gfud, const char *format, ...)
{
	gchar *error_message;
	va_list args;

	va_start(args, format);
	error_message = g_strdup_vprintf(format, args);
	va_end(args);

	gfud->callback(gfud, gfud->user_data, NULL, 0, error_message);
	g_free(error_message);
	gaim_util_fetch_url_cancel(gfud);
}

static void url_fetch_connect_cb(gpointer url_data, gint source, const gchar *error_message);

static gboolean
parse_redirect(const char *data, size_t data_len, gint sock,
			   GaimUtilFetchUrlData *gfud)
{
	gchar *s;

	if ((s = g_strstr_len(data, data_len, "Location: ")) != NULL)
	{
		gchar *new_url, *temp_url, *end;
		gboolean full;
		int len;

		s += strlen("Location: ");
		end = strchr(s, '\r');

		/* Just in case :) */
		if (end == NULL)
			end = strchr(s, '\n');

		if (end == NULL)
			return FALSE;

		len = end - s;

		new_url = g_malloc(len + 1);
		strncpy(new_url, s, len);
		new_url[len] = '\0';

		full = gfud->full;

		if (*new_url == '/' || g_strstr_len(new_url, len, "://") == NULL)
		{
			temp_url = new_url;

			new_url = g_strdup_printf("%s:%d%s", gfud->website.address,
									  gfud->website.port, temp_url);

			g_free(temp_url);

			full = FALSE;
		}

		gaim_debug_info("util", "Redirecting to %s\n", new_url);

		/*
		 * Try again, with this new location.  This code is somewhat
		 * ugly, but we need to reuse the gfud because whoever called
		 * us is holding a reference to it.
		 */
		g_free(gfud->url);
		gfud->url = new_url;
		gfud->full = full;
		g_free(gfud->request);
		gfud->request = NULL;

		g_free(gfud->website.user);
		g_free(gfud->website.passwd);
		g_free(gfud->website.address);
		g_free(gfud->website.page);
		gaim_url_parse(new_url, &gfud->website.address, &gfud->website.port,
					   &gfud->website.page, &gfud->website.user, &gfud->website.passwd);

		gfud->connect_data = gaim_proxy_connect(NULL, NULL,
				gfud->website.address, gfud->website.port,
				url_fetch_connect_cb, gfud);

		if (gfud->connect_data == NULL)
		{
			gaim_util_fetch_url_error(gfud, _("Unable to connect to %s"),
					gfud->website.address);
		}

		return TRUE;
	}

	return FALSE;
}

static size_t
parse_content_len(const char *data, size_t data_len)
{
	size_t content_len = 0;
	const char *p = NULL;

	/* This is still technically wrong, since headers are case-insensitive
	 * [RFC 2616, section 4.2], though this ought to catch the normal case.
	 * Note: data is _not_ nul-terminated.
	 */
	if(data_len > 16) {
		p = (strncmp(data, "Content-Length: ", 16) == 0) ? data : NULL;
		if(!p)
			p = (strncmp(data, "CONTENT-LENGTH: ", 16) == 0)
				? data : NULL;
		if(!p) {
			p = g_strstr_len(data, data_len, "\nContent-Length: ");
			if (p)
				p++;
		}
		if(!p) {
			p = g_strstr_len(data, data_len, "\nCONTENT-LENGTH: ");
			if (p)
				p++;
		}

		if(p)
			p += 16;
	}

	/* If we can find a Content-Length header at all, try to sscanf it.
	 * Response headers should end with at least \r\n, so sscanf is safe,
	 * if we make sure that there is indeed a \n in our header.
	 */
	if (p && g_strstr_len(p, data_len - (p - data), "\n")) {
		sscanf(p, "%" G_GSIZE_FORMAT, &content_len);
		gaim_debug_misc("util", "parsed %u\n", content_len);
	}

	return content_len;
}


static void
url_fetch_recv_cb(gpointer url_data, gint source, GaimInputCondition cond)
{
	GaimUtilFetchUrlData *gfud = url_data;
	int len;
	char buf[4096];
	char *data_cursor;
	gboolean got_eof = FALSE;

	while((len = read(source, buf, sizeof(buf))) > 0) {
		/* If we've filled up our buffer, make it bigger */
		if((gfud->len + len) >= gfud->data_len) {
			while((gfud->len + len) >= gfud->data_len)
				gfud->data_len += sizeof(buf);

			gfud->webdata = g_realloc(gfud->webdata, gfud->data_len);
		}

		data_cursor = gfud->webdata + gfud->len;

		gfud->len += len;

		memcpy(data_cursor, buf, len);

		gfud->webdata[gfud->len] = '\0';

		if(!gfud->got_headers) {
			char *tmp;

			/* See if we've reached the end of the headers yet */
			if((tmp = strstr(gfud->webdata, "\r\n\r\n"))) {
				char * new_data;
				guint header_len = (tmp + 4 - gfud->webdata);
				size_t content_len;

				gaim_debug_misc("util", "Response headers: '%.*s'\n",
					header_len, gfud->webdata);

				/* See if we can find a redirect. */
				if(parse_redirect(gfud->webdata, header_len, source, gfud))
					return;

				gfud->got_headers = TRUE;

				/* No redirect. See if we can find a content length. */
				content_len = parse_content_len(gfud->webdata, header_len);

				if(content_len == 0) {
					/* We'll stick with an initial 8192 */
					content_len = 8192;
				} else {
					gfud->has_explicit_data_len = TRUE;
				}


				/* If we're returning the headers too, we don't need to clean them out */
				if(gfud->include_headers) {
					gfud->data_len = content_len + header_len;
					gfud->webdata = g_realloc(gfud->webdata, gfud->data_len);
				} else {
					size_t body_len = 0;

					if(gfud->len > (header_len + 1))
						body_len = (gfud->len - header_len);

					content_len = MAX(content_len, body_len);

					new_data = g_try_malloc(content_len);
					if(new_data == NULL) {
						gaim_debug_error("util",
								"Failed to allocate %u bytes: %s\n",
								content_len, strerror(errno));
						gaim_util_fetch_url_error(gfud,
								_("Unable to allocate enough memory to hold "
								  "the contents from %s.  The web server may "
								  "be trying something malicious."),
								gfud->website.address);

						return;
					}

					/* We may have read part of the body when reading the headers, don't lose it */
					if(body_len > 0) {
						tmp += 4;
						memcpy(new_data, tmp, body_len);
					}

					/* Out with the old... */
					g_free(gfud->webdata);

					/* In with the new. */
					gfud->len = body_len;
					gfud->data_len = content_len;
					gfud->webdata = new_data;
				}
			}
		}

		if(gfud->has_explicit_data_len && gfud->len >= gfud->data_len) {
			got_eof = TRUE;
			break;
		}
	}

	if(len <= 0) {
		if(errno == EAGAIN) {
			return;
		} else if(errno != ETIMEDOUT) {
			got_eof = TRUE;
		} else {
			gaim_util_fetch_url_error(gfud, _("Error reading from %s: %s"),
					gfud->website.address, strerror(errno));
			return;
		}
	}

	if(got_eof) {
		gfud->webdata = g_realloc(gfud->webdata, gfud->len + 1);
		gfud->webdata[gfud->len] = '\0';

		gfud->callback(gfud, gfud->user_data, gfud->webdata, gfud->len, NULL);
		gaim_util_fetch_url_cancel(gfud);
	}
}

static void
url_fetch_send_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimUtilFetchUrlData *gfud;
	int len, total_len;

	gfud = data;

	total_len = strlen(gfud->request);

	len = write(gfud->fd, gfud->request + gfud->request_written,
			total_len - gfud->request_written);

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len < 0) {
		gaim_util_fetch_url_error(gfud, _("Error writing to %s: %s"),
				gfud->website.address, strerror(errno));
		return;
	}
	gfud->request_written += len;

	if (gfud->request_written != total_len)
		return;

	/* We're done writing our request, now start reading the response */
	gaim_input_remove(gfud->inpa);
	gfud->inpa = gaim_input_add(gfud->fd, GAIM_INPUT_READ, url_fetch_recv_cb,
		gfud);
}

static void
url_fetch_connect_cb(gpointer url_data, gint source, const gchar *error_message)
{
	GaimUtilFetchUrlData *gfud;

	gfud = url_data;
	gfud->connect_data = NULL;

	if (source == -1)
	{
		gaim_util_fetch_url_error(gfud, _("Unable to connect to %s: %s"),
				gfud->website.address, error_message);
		return;
	}

	gfud->fd = source;

	if (!gfud->request)
	{
		if (gfud->user_agent) {
			/* Host header is not forbidden in HTTP/1.0 requests, and HTTP/1.1
			 * clients must know how to handle the "chunked" transfer encoding.
			 * Gaim doesn't know how to handle "chunked", so should always send
			 * the Host header regardless, to get around some observed problems
			 */
			gfud->request = g_strdup_printf(
				"GET %s%s HTTP/%s\r\n"
				"Connection: close\r\n"
				"User-Agent: %s\r\n"
				"Accept: */*\r\n"
				"Host: %s\r\n\r\n",
				(gfud->full ? "" : "/"),
				(gfud->full ? gfud->url : gfud->website.page),
				(gfud->http11 ? "1.1" : "1.0"),
				gfud->user_agent, gfud->website.address);
		} else {
			gfud->request = g_strdup_printf(
				"GET %s%s HTTP/%s\r\n"
				"Connection: close\r\n"
				"Accept: */*\r\n"
				"Host: %s\r\n\r\n",
				(gfud->full ? "" : "/"),
				(gfud->full ? gfud->url : gfud->website.page),
				(gfud->http11 ? "1.1" : "1.0"),
				gfud->website.address);
		}
	}

	gaim_debug_misc("util", "Request: '%s'\n", gfud->request);

	gfud->inpa = gaim_input_add(source, GAIM_INPUT_WRITE,
								url_fetch_send_cb, gfud);
	url_fetch_send_cb(gfud, source, GAIM_INPUT_WRITE);
}

GaimUtilFetchUrlData *
gaim_util_fetch_url_request(const char *url, gboolean full,
		const char *user_agent, gboolean http11,
		const char *request, gboolean include_headers,
		GaimUtilFetchUrlCallback callback, void *user_data)
{
	GaimUtilFetchUrlData *gfud;

	g_return_val_if_fail(url      != NULL, NULL);
	g_return_val_if_fail(callback != NULL, NULL);

	gaim_debug_info("util",
			 "requested to fetch (%s), full=%d, user_agent=(%s), http11=%d\n",
			 url, full, user_agent?user_agent:"(null)", http11);

	gfud = g_new0(GaimUtilFetchUrlData, 1);

	gfud->callback = callback;
	gfud->user_data  = user_data;
	gfud->url = g_strdup(url);
	gfud->user_agent = g_strdup(user_agent);
	gfud->http11 = http11;
	gfud->full = full;
	gfud->request = g_strdup(request);
	gfud->include_headers = include_headers;

	gaim_url_parse(url, &gfud->website.address, &gfud->website.port,
				   &gfud->website.page, &gfud->website.user, &gfud->website.passwd);

	gfud->connect_data = gaim_proxy_connect(NULL, NULL,
			gfud->website.address, gfud->website.port,
			url_fetch_connect_cb, gfud);

	if (gfud->connect_data == NULL)
	{
		gaim_util_fetch_url_error(gfud, _("Unable to connect to %s"),
				gfud->website.address);
		return NULL;
	}

	return gfud;
}

void
gaim_util_fetch_url_cancel(GaimUtilFetchUrlData *gfud)
{
	if (gfud->connect_data != NULL)
		gaim_proxy_connect_cancel(gfud->connect_data);

	if (gfud->inpa > 0)
		gaim_input_remove(gfud->inpa);

	if (gfud->fd >= 0)
		close(gfud->fd);

	g_free(gfud->website.user);
	g_free(gfud->website.passwd);
	g_free(gfud->website.address);
	g_free(gfud->website.page);
	g_free(gfud->url);
	g_free(gfud->user_agent);
	g_free(gfud->request);
	g_free(gfud->webdata);

	g_free(gfud);
}

const char *
gaim_url_decode(const char *str)
{
	static char buf[BUF_LEN];
	guint i, j = 0;
	char *bum;
	char hex[3];

	g_return_val_if_fail(str != NULL, NULL);

	/*
	 * XXX - This check could be removed and buf could be made
	 * dynamically allocated, but this is easier.
	 */
	if (strlen(str) >= BUF_LEN)
		return NULL;

	for (i = 0; i < strlen(str); i++) {

		if (str[i] != '%')
			buf[j++] = str[i];
		else {
			strncpy(hex, str + ++i, 2);
			hex[2] = '\0';

			/* i is pointing to the start of the number */
			i++;

			/*
			 * Now it's at the end and at the start of the for loop
			 * will be at the next character.
			 */
			buf[j++] = strtol(hex, NULL, 16);
		}
	}

	buf[j] = '\0';

	if (!g_utf8_validate(buf, -1, (const char **)&bum))
		*bum = '\0';

	return buf;
}

const char *
gaim_url_encode(const char *str)
{
	const char *iter;
	static char buf[BUF_LEN];
	char utf_char[6];
	guint i, j = 0;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(g_utf8_validate(str, -1, NULL), NULL);

	iter = str;
	for (; *iter && j < (BUF_LEN - 1) ; iter = g_utf8_next_char(iter)) {
		gunichar c = g_utf8_get_char(iter);
		/* If the character is an ASCII character and is alphanumeric
		 * no need to escape */
		if (c < 128 && isalnum(c)) {
			buf[j++] = c;
		} else {
			int bytes = g_unichar_to_utf8(c, utf_char);
			for (i = 0; i < bytes; i++) {
				if (j > (BUF_LEN - 4))
					break;
				sprintf(buf + j, "%%%02x", utf_char[i] & 0xff);
				j += 3;
			}
		}
	}

	buf[j] = '\0';

	return buf;
}

/* Originally lifted from
 * http://www.oreillynet.com/pub/a/network/excerpt/spcookbook_chap03/index3.html
 * ... and slightly modified to be a bit more rfc822 compliant
 * ... and modified a bit more to make domain checking rfc1035 compliant
 *     with the exception permitted in rfc1101 for domains to start with digit
 *     but not completely checking to avoid conflicts with IP addresses
 */
gboolean
gaim_email_is_valid(const char *address)
{
	const char *c, *domain;
	static char *rfc822_specials = "()<>@,;:\\\"[]";

	/* first we validate the name portion (name@domain) (rfc822)*/
	for (c = address;  *c;  c++) {
		if (*c == '\"' && (c == address || *(c - 1) == '.' || *(c - 1) == '\"')) {
			while (*++c) {
				if (*c == '\\') {
					if (*c++ && *c < 127 && *c != '\n' && *c != '\r') continue;
					else return FALSE;
				}
				if (*c == '\"') break;
				if (*c < ' ' || *c >= 127) return FALSE;
			}
			if (!*c++) return FALSE;
			if (*c == '@') break;
			if (*c != '.') return FALSE;
			continue;
		}
		if (*c == '@') break;
		if (*c <= ' ' || *c >= 127) return FALSE;
		if (strchr(rfc822_specials, *c)) return FALSE;
	}
	/* strictly we should return false if (*(c - 1) == '.') too, but I think
	 * we should permit user.@domain type addresses - they do work :) */
	if (c == address) return FALSE;

	/* next we validate the domain portion (name@domain) (rfc1035 & rfc1011) */
	if (!*(domain = ++c)) return FALSE;
	do {
		if (*c == '.' && (c == domain || *(c - 1) == '.' || *(c - 1) == '-'))
			return FALSE;
		if (*c == '-' && *(c - 1) == '.') return FALSE;
		if ((*c < '0' && *c != '-' && *c != '.') || (*c > '9' && *c < 'A') ||
			(*c > 'Z' && *c < 'a') || (*c > 'z')) return FALSE;
	} while (*++c);

	if (*(c - 1) == '-') return FALSE;

	return ((c - domain) > 3 ? TRUE : FALSE);
}

/* Stolen from gnome_uri_list_extract_uris */
GList *
gaim_uri_list_extract_uris(const gchar *uri_list)
{
	const gchar *p, *q;
	gchar *retval;
	GList *result = NULL;

	g_return_val_if_fail (uri_list != NULL, NULL);

	p = uri_list;

	/* We don't actually try to validate the URI according to RFC
	* 2396, or even check for allowed characters - we just ignore
	* comments and trim whitespace off the ends.  We also
	* allow LF delimination as well as the specified CRLF.
	*/
	while (p) {
		if (*p != '#') {
			while (isspace(*p))
				p++;

			q = p;
			while (*q && (*q != '\n') && (*q != '\r'))
				q++;

			if (q > p) {
				q--;
				while (q > p && isspace(*q))
					q--;

				retval = (gchar*)g_malloc (q - p + 2);
				strncpy (retval, p, q - p + 1);
				retval[q - p + 1] = '\0';

				result = g_list_prepend (result, retval);
			}
		}
		p = strchr (p, '\n');
		if (p)
			p++;
	}

	return g_list_reverse (result);
}


/* Stolen from gnome_uri_list_extract_filenames */
GList *
gaim_uri_list_extract_filenames(const gchar *uri_list)
{
	GList *tmp_list, *node, *result;

	g_return_val_if_fail (uri_list != NULL, NULL);

	result = gaim_uri_list_extract_uris(uri_list);

	tmp_list = result;
	while (tmp_list) {
		gchar *s = (gchar*)tmp_list->data;

		node = tmp_list;
		tmp_list = tmp_list->next;

		if (!strncmp (s, "file:", 5)) {
			node->data = g_filename_from_uri (s, NULL, NULL);
			/* not sure if this fallback is useful at all */
			if (!node->data) node->data = g_strdup (s+5);
		} else {
			result = g_list_remove_link(result, node);
			g_list_free_1 (node);
		}
		g_free (s);
	}
	return result;
}

/**************************************************************************
 * UTF8 String Functions
 **************************************************************************/
gchar *
gaim_utf8_try_convert(const char *str)
{
	gsize converted;
	gchar *utf8;

	g_return_val_if_fail(str != NULL, NULL);

	if (g_utf8_validate(str, -1, NULL)) {
		return g_strdup(str);
	}

	utf8 = g_locale_to_utf8(str, -1, &converted, NULL, NULL);
	if (utf8 != NULL)
		return utf8;

	utf8 = g_convert(str, -1, "UTF-8", "ISO-8859-15", &converted, NULL, NULL);
	if ((utf8 != NULL) && (converted == strlen(str)))
		return utf8;

	g_free(utf8);

	return NULL;
}

#define utf8_first(x) ((x & 0x80) == 0 || (x & 0xe0) == 0xc0 \
		       || (x & 0xf0) == 0xe0 || (x & 0xf8) == 0xf)
gchar *
gaim_utf8_salvage(const char *str)
{
	GString *workstr;
	const char *end;

	g_return_val_if_fail(str != NULL, NULL);

	workstr = g_string_sized_new(strlen(str));

	do {
		g_utf8_validate(str, -1, &end);
		workstr = g_string_append_len(workstr, str, end - str);
		str = end;
		if (*str == '\0')
			break;
		do {
			workstr = g_string_append_c(workstr, '?');
			str++;
		} while (!utf8_first(*str));
	} while (*str != '\0');

	return g_string_free(workstr, FALSE);
}


char *
gaim_utf8_ncr_encode(const char *str)
{
	GString *out;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(g_utf8_validate(str, -1, NULL), NULL);

	out = g_string_new("");

	for(; *str; str = g_utf8_next_char(str)) {
		gunichar wc = g_utf8_get_char(str);

		/* super simple check. hopefully not too wrong. */
		if(wc >= 0x80) {
			g_string_append_printf(out, "&#%u;", (guint32) wc);
		} else {
			g_string_append_unichar(out, wc);
		}
	}

	return g_string_free(out, FALSE);
}


char *
gaim_utf8_ncr_decode(const char *str)
{
	GString *out;
	char *buf, *b;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(g_utf8_validate(str, -1, NULL), NULL);

	buf = (char *) str;
	out = g_string_new("");

	while( (b = strstr(buf, "&#")) ) {
		gunichar wc;
		int base = 0;

		/* append everything leading up to the &# */
		g_string_append_len(out, buf, b-buf);

		b += 2; /* skip past the &# */

		/* strtoul will treat 0x prefix as hex, but not just x */
		if(*b == 'x' || *b == 'X') {
			base = 16;
			b++;
		}

		/* advances buf to the end of the ncr segment */
		wc = (gunichar) strtoul(b, &buf, base);

		/* this mimics the previous impl of ncr_decode */
		if(*buf == ';') {
			g_string_append_unichar(out, wc);
			buf++;
		}
	}

	/* append whatever's left */
	g_string_append(out, buf);

	return g_string_free(out, FALSE);
}


int
gaim_utf8_strcasecmp(const char *a, const char *b)
{
	char *a_norm = NULL;
	char *b_norm = NULL;
	int ret = -1;

	if(!a && b)
		return -1;
	else if(!b && a)
		return 1;
	else if(!a && !b)
		return 0;

	if(!g_utf8_validate(a, -1, NULL) || !g_utf8_validate(b, -1, NULL))
	{
		gaim_debug_error("gaim_utf8_strcasecmp",
						 "One or both parameters are invalid UTF8\n");
		return ret;
	}

	a_norm = g_utf8_casefold(a, -1);
	b_norm = g_utf8_casefold(b, -1);
	ret = g_utf8_collate(a_norm, b_norm);
	g_free(a_norm);
	g_free(b_norm);

	return ret;
}

/* previously conversation::find_nick() */
gboolean
gaim_utf8_has_word(const char *haystack, const char *needle)
{
	char *hay, *pin, *p;
	int n;
	gboolean ret = FALSE;

	hay = g_utf8_strdown(haystack, -1);

	pin = g_utf8_strdown(needle, -1);
	n = strlen(pin);

	if ((p = strstr(hay, pin)) != NULL) {
		if ((p == hay || !isalnum(*(p - 1))) && !isalnum(*(p + n))) {
			ret = TRUE;
		}
	}

	g_free(pin);
	g_free(hay);

	return ret;
}

void
gaim_print_utf8_to_console(FILE *filestream, char *message)
{
	gchar *message_conv;
	GError *error = NULL;

	/* Try to convert 'message' to user's locale */
	message_conv = g_locale_from_utf8(message, -1, NULL, NULL, &error);
	if (message_conv != NULL) {
		fputs(message_conv, filestream);
		g_free(message_conv);
	}
	else
	{
		/* use 'message' as a fallback */
		g_warning("%s\n", error->message);
		g_error_free(error);
		fputs(message, filestream);
	}
}

gboolean gaim_message_meify(char *message, size_t len)
{
	char *c;
	gboolean inside_html = FALSE;

	g_return_val_if_fail(message != NULL, FALSE);

	if(len == -1)
		len = strlen(message);

	for (c = message; *c; c++, len--) {
		if(inside_html) {
			if(*c == '>')
				inside_html = FALSE;
		} else {
			if(*c == '<')
				inside_html = TRUE;
			else
				break;
		}
	}

	if(*c && !g_ascii_strncasecmp(c, "/me ", 4)) {
		memmove(c, c+4, len-3);
		return TRUE;
	}

	return FALSE;
}

char *gaim_text_strip_mnemonic(const char *in)
{
	char *out;
	char *a;
	char *a0;
	const char *b;

	g_return_val_if_fail(in != NULL, NULL);

	out = g_malloc(strlen(in)+1);
	a = out;
	b = in;

	a0 = a; /* The last non-space char seen so far, or the first char */

	while(*b) {
		if(*b == '_') {
			if(a > out && b > in && *(b-1) == '(' && *(b+1) && !(*(b+1) & 0x80) && *(b+2) == ')') {
				/* Detected CJK style shortcut (Bug 875311) */
				a = a0;	/* undo the left parenthesis */
				b += 3;	/* and skip the whole mess */
			} else if(*(b+1) == '_') {
				*(a++) = '_';
				b += 2;
				a0 = a;
			} else {
				b++;
			}
		/* We don't want to corrupt the middle of UTF-8 characters */
		} else if (!(*b & 0x80)) {	/* other 1-byte char */
			if (*b != ' ')
				a0 = a;
			*(a++) = *(b++);
		} else {
			/* Multibyte utf8 char, don't look for _ inside these */
			int n = 0;
			int i;
			if ((*b & 0xe0) == 0xc0) {
				n = 2;
			} else if ((*b & 0xf0) == 0xe0) {
				n = 3;
			} else if ((*b & 0xf8) == 0xf0) {
				n = 4;
			} else if ((*b & 0xfc) == 0xf8) {
				n = 5;
			} else if ((*b & 0xfe) == 0xfc) {
				n = 6;
			} else {		/* Illegal utf8 */
				n = 1;
			}
			a0 = a; /* unless we want to delete CJK spaces too */
			for (i = 0; i < n && *b; i += 1) {
				*(a++) = *(b++);
			}
		}
	}
	*a = '\0';

	return out;
}

const char* gaim_unescape_filename(const char *escaped) {
	return gaim_url_decode(escaped);
}


/* this is almost identical to gaim_url_encode (hence gaim_url_decode
 * being used above), but we want to keep certain characters unescaped
 * for compat reasons */
const char *
gaim_escape_filename(const char *str)
{
	const char *iter;
	static char buf[BUF_LEN];
	char utf_char[6];
	guint i, j = 0;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(g_utf8_validate(str, -1, NULL), NULL);

	iter = str;
	for (; *iter && j < (BUF_LEN - 1) ; iter = g_utf8_next_char(iter)) {
		gunichar c = g_utf8_get_char(iter);
		/* If the character is an ASCII character and is alphanumeric,
		 * or one of the specified values, no need to escape */
		if (c < 128 && (isalnum(c) || c == '@' || c == '-' ||
				c == '_' || c == '.' || c == '#')) {
			buf[j++] = c;
		} else {
			int bytes = g_unichar_to_utf8(c, utf_char);
			for (i = 0; i < bytes; i++) {
				if (j > (BUF_LEN - 4))
					break;
				sprintf(buf + j, "%%%02x", utf_char[i] & 0xff);
				j += 3;
			}
		}
	}

	buf[j] = '\0';

	return buf;
}

