/**
 * @file msn-utils.c Utility functions
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
#include "msn.h"
#include "msn-utils.h"
#include "time.h"
//#include <openssl/md5.h>

char *rand_guid(void);

/**************************************************************************
 * Util
 **************************************************************************/
char *
rand_guid()
{
	return g_strdup_printf("%4X%4X-%4X-%4X-%4X-%4X%4X%4X",
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111);
}

void
msn_parse_format(const char *mime, char **pre_ret, char **post_ret)
{
	char *cur;
	GString *pre  = g_string_new(NULL);
	GString *post = g_string_new(NULL);
	unsigned int colors[3];

	if (pre_ret  != NULL) *pre_ret  = NULL;
	if (post_ret != NULL) *post_ret = NULL;

	cur = strstr(mime, "FN=");

	if (cur && (*(cur = cur + 3) != ';'))
	{
		pre = g_string_append(pre, "<FONT FACE=\"");

		while (*cur && *cur != ';')
		{
			pre = g_string_append_c(pre, *cur);
			cur++;
		}

		pre = g_string_append(pre, "\">");
		post = g_string_prepend(post, "</FONT>");
	}

	cur = strstr(mime, "EF=");

	if (cur && (*(cur = cur + 3) != ';'))
	{
		while (*cur && *cur != ';')
		{
			pre = g_string_append_c(pre, '<');
			pre = g_string_append_c(pre, *cur);
			pre = g_string_append_c(pre, '>');
			post = g_string_prepend_c(post, '>');
			post = g_string_prepend_c(post, *cur);
			post = g_string_prepend_c(post, '/');
			post = g_string_prepend_c(post, '<');
			cur++;
		}
	}

	cur = strstr(mime, "CO=");

	if (cur && (*(cur = cur + 3) != ';'))
	{
		int i;

		i = sscanf(cur, "%02x%02x%02x;", &colors[0], &colors[1], &colors[2]);

		if (i > 0)
		{
			char tag[64];

			if (i == 1)
			{
				colors[1] = 0;
				colors[2] = 0;
			}
			else if (i == 2)
			{
				unsigned int temp = colors[0];

				colors[0] = colors[1];
				colors[1] = temp;
				colors[2] = 0;
			}
			else if (i == 3)
			{
				unsigned int temp = colors[2];

				colors[2] = colors[0];
				colors[0] = temp;
			}

			g_snprintf(tag, sizeof(tag),
					   "<FONT COLOR=\"#%02hhx%02hhx%02hhx\">",
					   colors[0], colors[1], colors[2]);

			pre = g_string_append(pre, tag);
			post = g_string_prepend(post, "</FONT>");
		}
	}

	cur = g_strdup(gaim_url_decode(pre->str));
	g_string_free(pre, TRUE);

	if (pre_ret != NULL)
		*pre_ret = cur;
	else
		g_free(cur);

	cur = g_strdup(gaim_url_decode(post->str));
	g_string_free(post, TRUE);

	if (post_ret != NULL)
		*post_ret = cur;
	else
		g_free(cur);
}

/*encode the str to RFC2047 style
 * Currently only support the UTF-8 and base64 encode
 */
char *
msn_encode_mime(const char *str)
{
	char *base64;
	
	base64 = gaim_base64_encode((guchar *)str, strlen(str));
	return g_strdup_printf("=?utf-8?B?%s?=", base64);
}

/*
 * We need this because we're only supposed to encode spaces in the font
 * names. gaim_url_encode() isn't acceptable.
 */
static const char *
encode_spaces(const char *str)
{
	static char buf[BUF_LEN];
	const char *c;
	char *d;

	g_return_val_if_fail(str != NULL, NULL);

	for (c = str, d = buf; *c != '\0'; c++)
	{
		if (*c == ' ')
		{
			*d++ = '%';
			*d++ = '2';
			*d++ = '0';
		}
		else
			*d++ = *c;
	}

	return buf;
}

/*
 * Taken from the zephyr plugin.
 * This parses HTML formatting (put out by one of the gtkimhtml widgets
 * and converts it to msn formatting. It doesn't deal with the tag closing,
 * but gtkimhtml widgets give valid html.
 * It currently deals properly with <b>, <u>, <i>, <font face=...>,
 * <font color=...>.
 * It ignores <font back=...> and <font size=...>
 */
void
msn_import_html(const char *html, char **attributes, char **message)
{
	int len, retcount = 0;
	const char *c;
	char *msg;
	char *fontface = NULL;
	char fonteffect[4];
	char fontcolor[7];

	g_return_if_fail(html       != NULL);
	g_return_if_fail(attributes != NULL);
	g_return_if_fail(message    != NULL);

	len = strlen(html);
	msg = g_malloc0(len + 1);

	memset(fontcolor, 0, sizeof(fontcolor));
	strcat(fontcolor, "0");
	memset(fonteffect, 0, sizeof(fonteffect));

	for (c = html; *c != '\0';)
	{
		if (*c == '<')
		{
			if (!g_ascii_strncasecmp(c + 1, "br>", 3))
			{
				msg[retcount++] = '\r';
				msg[retcount++] = '\n';
				c += 4;
			}
			else if (!g_ascii_strncasecmp(c + 1, "i>", 2))
			{
				strcat(fonteffect, "I");
				c += 3;
			}
			else if (!g_ascii_strncasecmp(c + 1, "b>", 2))
			{
				strcat(fonteffect, "B");
				c += 3;
			}
			else if (!g_ascii_strncasecmp(c + 1, "u>", 2))
			{
				strcat(fonteffect, "U");
				c += 3;
			}
			else if (!g_ascii_strncasecmp(c + 1, "s>", 2))
			{
				strcat(fonteffect, "S");
				c += 3;
			}
			else if (!g_ascii_strncasecmp(c + 1, "a href=\"", 8))
			{
				c += 9;

				if (!g_ascii_strncasecmp(c, "mailto:", 7))
					c += 7;

				while ((*c != '\0') && g_ascii_strncasecmp(c, "\">", 2))
					msg[retcount++] = *c++;

				if (*c != '\0')
					c += 2;

				/* ignore descriptive string */
				while ((*c != '\0') && g_ascii_strncasecmp(c, "</a>", 4))
					c++;

				if (*c != '\0')
					c += 4;
			}
			else if (!g_ascii_strncasecmp(c + 1, "font", 4))
			{
				c += 5;

				while ((*c != '\0') && !g_ascii_strncasecmp(c, " ", 1))
					c++;

				if (!g_ascii_strncasecmp(c, "color=\"#", 7))
				{
					c += 8;

					fontcolor[0] = *(c + 4);
					fontcolor[1] = *(c + 5);
					fontcolor[2] = *(c + 2);
					fontcolor[3] = *(c + 3);
					fontcolor[4] = *c;
					fontcolor[5] = *(c + 1);

					c += 8;
				}
				else if (!g_ascii_strncasecmp(c, "face=\"", 6))
				{
					const char *end = NULL;
					const char *comma = NULL;
					unsigned int namelen = 0;

					c += 6;
					end = strchr(c, '\"');
					comma = strchr(c, ',');

					if (comma == NULL || comma > end)
						namelen = (unsigned int)(end - c);
					else
						namelen = (unsigned int)(comma - c);

					fontface = g_strndup(c, namelen);
					c = end + 2;
				}
				else
				{
					/* Drop all unrecognized/misparsed font tags */
					while ((*c != '\0') && g_ascii_strncasecmp(c, "\">", 2))
						c++;

					if (*c != '\0')
						c += 2;
				}
			}
			else
			{
				while ((*c != '\0') && (*c != '>'))
					c++;
				if (*c != '\0')
					c++;
			}
		}
		else if (*c == '&')
		{
			if (!g_ascii_strncasecmp(c, "&lt;", 4))
			{
				msg[retcount++] = '<';
				c += 4;
			}
			else if (!g_ascii_strncasecmp(c, "&gt;", 4))
			{
				msg[retcount++] = '>';
				c += 4;
			}
			else if (!g_ascii_strncasecmp(c, "&nbsp;", 6))
			{
				msg[retcount++] = ' ';
				c += 6;
			}
			else if (!g_ascii_strncasecmp(c, "&quot;", 6))
			{
				msg[retcount++] = '"';
				c += 6;
			}
			else if (!g_ascii_strncasecmp(c, "&amp;", 5))
			{
				msg[retcount++] = '&';
				c += 5;
			}
			else if (!g_ascii_strncasecmp(c, "&apos;", 6))
			{
				msg[retcount++] = '\'';
				c += 6;
			}
			else
				msg[retcount++] = *c++;
		}
		else
			msg[retcount++] = *c++;
	}

	if (fontface == NULL)
		fontface = g_strdup("MS Sans Serif");

	*attributes = g_strdup_printf("FN=%s; EF=%s; CO=%s; PF=0",
								  encode_spaces(fontface),
								  fonteffect, fontcolor);
	*message = g_strdup(msg);

	g_free(fontface);
	g_free(msg);
}

void
msn_parse_socket(const char *str, char **ret_host, int *ret_port)
{
	char *host;
	char *c;
	int port;

	host = g_strdup(str);

	if ((c = strchr(host, ':')) != NULL){
		*c = '\0';
		port = atoi(c + 1);
	}else{
		port = 1863;
	}

	*ret_host = host;
	*ret_port = port;
}
/***************************************************************************
 * MSN Time Related Funciton
 ***************************************************************************/
#if 0
int
msn_convert_iso8601(const char *timestr,struct tm tm_time)
{
	char temp[64];
	struct tm ctime;
	time_t ts;

	gaim_debug_info("MaYuan","convert string is{%s}\n",timestr);
	tzset();
	/*copy string first*/
	memset(temp, 0, sizeof(temp));
	strncpy(temp, timestr, strlen(timestr));

	/*convert via strptime()*/
	memset(&ctime, 0, sizeof(struct tm));
	strptime(temp, "%d %b %Y %T %Z", &ctime);
	ts = mktime(&ctime) - timezone;
	localtime_r(&ts, tm_time);
}
#endif

/***************************************************************************
 * MSN Challenge Computing Function
 ***************************************************************************/
/*check the edian of system*/
int 
isBigEndian(void)
{
		short int word = 0x0100;
		char *byte = (char *)&word;

		return(byte[0]);
}

/*swap utility*/
unsigned int 
swapInt(unsigned int dw)
{
		unsigned int tmp;
		tmp =  (dw & 0x000000FF);
		tmp = ((dw & 0x0000FF00) >> 0x08) | (tmp << 0x08);
		tmp = ((dw & 0x00FF0000) >> 0x10) | (tmp << 0x08);
		tmp = ((dw & 0xFF000000) >> 0x18) | (tmp << 0x08);
		return(tmp);
}

/*
 * Handle MSN Chanllege computation
 *This algorithm reference with http://msnpiki.msnfanatic.com/index.php/MSNP11:Challenges
 */
#define BUFSIZE	256
void 
msn_handle_chl(char *input, char *output)
{
		GaimCipher *cipher;
		GaimCipherContext *context;
		char *productKey = MSNP13_WLM_PRODUCT_KEY,
			 *productID  = MSNP13_WLM_PRODUCT_ID,
			 *hexChars   = "0123456789abcdef",
			 buf[BUFSIZE];
		unsigned char md5Hash[16], *newHash;
		unsigned int *md5Parts, *chlStringParts, newHashParts[5];

		long long nHigh=0, nLow=0;

		int i, bigEndian;

		/* Determine our endianess */
		bigEndian = isBigEndian();

		/* Create the MD5 hash by using Gaim MD5 algorithm*/
		cipher = gaim_ciphers_find_cipher("md5");
		context = gaim_cipher_context_new(cipher, NULL);

		gaim_cipher_context_append(context, (const guchar *)input,
						strlen(input));
		gaim_cipher_context_append(context, (const guchar *)productKey,
						strlen(productKey));
		gaim_cipher_context_digest(context, sizeof(md5Hash), md5Hash, NULL);
		gaim_cipher_context_destroy(context);

		/* Split it into four integers */
		md5Parts = (unsigned int *)md5Hash;
		for(i=0; i<4; i++){  
				/* check for endianess */
				if(bigEndian)
						md5Parts[i] = swapInt(md5Parts[i]);

				/* & each integer with 0x7FFFFFFF          */
				/* and save one unmodified array for later */
				newHashParts[i] = md5Parts[i];
				md5Parts[i] &= 0x7FFFFFFF;
		}

		/* make a new string and pad with '0' */
		snprintf(buf, BUFSIZE-5, "%s%s", input, productID);
		i = strlen(buf);
		memset(&buf[i], '0', 8 - (i % 8));
		buf[i + (8 - (i % 8))]='\0';

		/* split into integers */
		chlStringParts = (unsigned int *)buf;

		/* this is magic */
		for (i=0; i<(strlen(buf)/4)-1; i+=2){
				long long temp;

				if(bigEndian){
						chlStringParts[i]   = swapInt(chlStringParts[i]);
						chlStringParts[i+1] = swapInt(chlStringParts[i+1]);
				}

				temp=(md5Parts[0] * (((0x0E79A9C1 * (long long)chlStringParts[i]) % 0x7FFFFFFF)+nHigh) + md5Parts[1])%0x7FFFFFFF;
				nHigh=(md5Parts[2] * (((long long)chlStringParts[i+1]+temp) % 0x7FFFFFFF) + md5Parts[3]) % 0x7FFFFFFF;
				nLow=nLow + nHigh + temp;
		}
		nHigh=(nHigh+md5Parts[1]) % 0x7FFFFFFF;
		nLow=(nLow+md5Parts[3]) % 0x7FFFFFFF;

		newHashParts[0]^=nHigh;
		newHashParts[1]^=nLow;
		newHashParts[2]^=nHigh;
		newHashParts[3]^=nLow;

		/* swap more bytes if big endian */
		for(i=0; i<4 && bigEndian; i++)
				newHashParts[i] = swapInt(newHashParts[i]); 

		/* make a string of the parts */
		newHash = (unsigned char *)newHashParts;

		/* convert to hexadecimal */
		for (i=0; i<16; i++)
		{
				output[i*2]=hexChars[(newHash[i]>>4)&0xF];
				output[(i*2)+1]=hexChars[newHash[i]&0xF];
		}

		output[32]='\0';

//		gaim_debug_info("MaYuan","chl output{%s}\n",output);
}

#if (!defined(_XOPEN_SOURCE))||defined(_WIN32)

#ifndef __P
# if defined (__GNUC__) || (defined (__STDC__) && __STDC__)
#  define __P(args) args
# else
#  define __P(args) ()
# endif  /* GCC.  */
#endif  /* Not __P.  */

#if defined(HAVE_LOCALTIME_R) && ! HAVE_LOCALTIME_R && ! defined localtime_r
# ifdef _LIBC
#  define localtime_r __localtime_r
# else
/* Approximate localtime_r as best we can in its absence.  */
#  define localtime_r my_localtime_r
static struct tm *localtime_r __P ((const time_t *, struct tm *));
static struct tm *
localtime_r (t, tp)
     const time_t *t;
     struct tm *tp;
{
  struct tm *l = localtime (t);
  if (! l)
    return 0;
  *tp = *l;
  return tp;
}
# endif /* ! _LIBC */
#endif /* ! HAVE_LOCALTIME_R && ! defined (localtime_r) */


#define match_char(ch1, ch2) if (ch1 != ch2) return NULL

#if defined __GNUC__ && __GNUC__ >= 2
# define match_string(cs1, s2) \
  ({ size_t len = strlen (cs1);						      \
     int result = strncasecmp ((cs1), (s2), len) == 0;			      \
     if (result) (s2) += len;						      \
     result; })
#else
/* Oh come on.  Get a reasonable compiler.  */
# define match_string(cs1, s2) \
  (strncasecmp ((cs1), (s2), strlen (cs1)) ? 0 : ((s2) += strlen (cs1), 1))
#endif

/* We intentionally do not use isdigit() for testing because this will
   lead to problems with the wide character version.  */
#define get_number(from, to, n) \
  do {									      \
    int __n = n;							      \
    val = 0;								      \
    while (*rp == ' ')							      \
      ++rp;								      \
    if ((*rp < '0') || (*rp > '9'))						      \
      return NULL;							      \
    do {								      \
      val *= 10;							      \
      val += *rp++ - '0';						      \
    } while ((--__n > 0) && (val * 10 <= to) && (*rp >= '0') && (*rp <= '9'));	      \
    if ((val < from) || (val > to))						      \
      return NULL;							      \
  } while (0)

#ifdef _NL_CURRENT
# define get_alt_number(from, to, n) \
  ({									      \
    __label__ do_normal;						      \
    if (*decided != raw)						      \
      {									      \
	const char *alts = _NL_CURRENT (LC_TIME, ALT_DIGITS);		      \
	int __n = n;							      \
	int any = 0;							      \
	while (*rp == ' ')						      \
	  ++rp;								      \
	val = 0;							      \
	do {								      \
	  val *= 10;							      \
	  while (*alts != '\0')						      \
	    {								      \
	      size_t len = strlen (alts);				      \
	      if (strncasecmp (alts, rp, len) == 0)			      \
	        break;							      \
	      alts += len + 1;						      \
	      ++val;							      \
	    }								      \
	  if (*alts == '\0')						      \
	    {								      \
	      if (*decided == not && ! any)				      \
		goto do_normal;						      \
	      /* If we haven't read anything it's an error.  */		      \
	      if (! any)						      \
		return NULL;						      \
	      /* Correct the premature multiplication.  */		      \
	      val /= 10;						      \
	      break;							      \
	    }								      \
	  else								      \
	    *decided = loc;						      \
	} while (--__n > 0 && val * 10 <= to);				      \
	if (val < from || val > to)					      \
	  return NULL;							      \
      }									      \
    else								      \
      {									      \
       do_normal:							      \
        get_number (from, to, n);					      \
      }									      \
    0;									      \
  })
#else
# define get_alt_number(from, to, n) \
  /* We don't have the alternate representation.  */			      \
  get_number(from, to, n)
#endif

#define recursive(new_fmt) \
  (*(new_fmt) != '\0'							      \
   && (rp = strptime_internal (rp, (new_fmt), tm, decided, era_cnt)) != NULL)


#ifdef _LIBC
/* This is defined in locale/C-time.c in the GNU libc.  */
extern const struct locale_data _nl_C_LC_TIME;
extern const unsigned short int __mon_yday[2][13];

# define weekday_name (&_nl_C_LC_TIME.values[_NL_ITEM_INDEX (DAY_1)].string)
# define ab_weekday_name \
  (&_nl_C_LC_TIME.values[_NL_ITEM_INDEX (ABDAY_1)].string)
# define month_name (&_nl_C_LC_TIME.values[_NL_ITEM_INDEX (MON_1)].string)
# define ab_month_name (&_nl_C_LC_TIME.values[_NL_ITEM_INDEX (ABMON_1)].string)
# define HERE_D_T_FMT (_nl_C_LC_TIME.values[_NL_ITEM_INDEX (D_T_FMT)].string)
# define HERE_D_FMT (_nl_C_LC_TIME.values[_NL_ITEM_INDEX (D_FMT)].string)
# define HERE_AM_STR (_nl_C_LC_TIME.values[_NL_ITEM_INDEX (AM_STR)].string)
# define HERE_PM_STR (_nl_C_LC_TIME.values[_NL_ITEM_INDEX (PM_STR)].string)
# define HERE_T_FMT_AMPM \
  (_nl_C_LC_TIME.values[_NL_ITEM_INDEX (T_FMT_AMPM)].string)
# define HERE_T_FMT (_nl_C_LC_TIME.values[_NL_ITEM_INDEX (T_FMT)].string)

# define strncasecmp(s1, s2, n) __strncasecmp (s1, s2, n)
#else
static char const weekday_name[][10] =
  {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
  };
static char const ab_weekday_name[][4] =
  {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
  };
static char const month_name[][10] =
  {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
  };
static char const ab_month_name[][4] =
  {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
# define HERE_D_T_FMT "%a %b %e %H:%M:%S %Y"
# define HERE_D_FMT "%m/%d/%y"
# define HERE_AM_STR "AM"
# define HERE_PM_STR "PM"
# define HERE_T_FMT_AMPM "%I:%M:%S %p"
# define HERE_T_FMT "%H:%M:%S"

const unsigned short int __mon_yday[2][13] =
  {
    /* Normal years.  */
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    /* Leap years.  */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
  };
#endif

/* Status of lookup: do we use the locale data or the raw data?  */
enum locale_status { not, loc, raw };


#ifndef __isleap
/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
# define __isleap(year)	\
  ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#endif

/* Compute the day of the week.  */
static void
day_of_the_week (struct tm *tm)
{
  /* We know that January 1st 1970 was a Thursday (= 4).  Compute the
     the difference between this data in the one on TM and so determine
     the weekday.  */
  int corr_year = 1900 + tm->tm_year - (tm->tm_mon < 2);
  int wday = (-473
	      + (365 * (tm->tm_year - 70))
	      + (corr_year / 4)
	      - ((corr_year / 4) / 25) + ((corr_year / 4) % 25 < 0)
	      + (((corr_year / 4) / 25) / 4)
	      + __mon_yday[0][tm->tm_mon]
	      + tm->tm_mday - 1);
  tm->tm_wday = ((wday % 7) + 7) % 7;
}

/* Compute the day of the year.  */
static void
day_of_the_year (struct tm *tm)
{
  tm->tm_yday = (__mon_yday[__isleap (1900 + tm->tm_year)][tm->tm_mon]
		 + (tm->tm_mday - 1));
}

static char *
#ifdef _LIBC
internal_function
#endif
strptime_internal __P ((const char *rp, const char *fmt, struct tm *tm,
			enum locale_status *decided, int era_cnt));

static char *
#ifdef _LIBC
internal_function
#endif
strptime_internal (rp, fmt, tm, decided, era_cnt)
     const char *rp;
     const char *fmt;
     struct tm *tm;
     enum locale_status *decided;
     int era_cnt;
{
  const char *rp_backup;
  int cnt;
  size_t val;
  int have_I, is_pm;
  int century, want_century;
  int want_era;
  int have_wday, want_xday;
  int have_yday;
  int have_mon, have_mday;
#ifdef _NL_CURRENT
  size_t num_eras;
#endif
  struct era_entry *era;

  have_I = is_pm = 0;
  century = -1;
  want_century = 0;
  want_era = 0;
  era = NULL;

  have_wday = want_xday = have_yday = have_mon = have_mday = 0;

  while (*fmt != '\0')
    {
      /* A white space in the format string matches 0 more or white
	 space in the input string.  */
      if (isspace (*fmt))
	{
	  while (isspace (*rp))
	    ++rp;
	  ++fmt;
	  continue;
	}

      /* Any character but `%' must be matched by the same character
	 in the iput string.  */
      if (*fmt != '%')
	{
	  match_char (*fmt++, *rp++);
	  continue;
	}

      ++fmt;
#ifndef _NL_CURRENT
      /* We need this for handling the `E' modifier.  */
    start_over:
#endif

      /* Make back up of current processing pointer.  */
      rp_backup = rp;

      switch (*fmt++)
	{
	case '%':
	  /* Match the `%' character itself.  */
	  match_char ('%', *rp++);
	  break;
	case 'a':
	case 'A':
	  /* Match day of week.  */
	  for (cnt = 0; cnt < 7; ++cnt)
	    {
#ifdef _NL_CURRENT
	      if (*decided !=raw)
		{
		  if (match_string (_NL_CURRENT (LC_TIME, DAY_1 + cnt), rp))
		    {
		      if (*decided == not
			  && strcmp (_NL_CURRENT (LC_TIME, DAY_1 + cnt),
				     weekday_name[cnt]))
			*decided = loc;
		      break;
		    }
		  if (match_string (_NL_CURRENT (LC_TIME, ABDAY_1 + cnt), rp))
		    {
		      if (*decided == not
			  && strcmp (_NL_CURRENT (LC_TIME, ABDAY_1 + cnt),
				     ab_weekday_name[cnt]))
			*decided = loc;
		      break;
		    }
		}
#endif
	      if (*decided != loc
		  && (match_string (weekday_name[cnt], rp)
		      || match_string (ab_weekday_name[cnt], rp)))
		{
		  *decided = raw;
		  break;
		}
	    }
	  if (cnt == 7)
	    /* Does not match a weekday name.  */
	    return NULL;
	  tm->tm_wday = cnt;
	  have_wday = 1;
	  break;
	case 'b':
	case 'B':
	case 'h':
	  /* Match month name.  */
	  for (cnt = 0; cnt < 12; ++cnt)
	    {
#ifdef _NL_CURRENT
	      if (*decided !=raw)
		{
		  if (match_string (_NL_CURRENT (LC_TIME, MON_1 + cnt), rp))
		    {
		      if (*decided == not
			  && strcmp (_NL_CURRENT (LC_TIME, MON_1 + cnt),
				     month_name[cnt]))
			*decided = loc;
		      break;
		    }
		  if (match_string (_NL_CURRENT (LC_TIME, ABMON_1 + cnt), rp))
		    {
		      if (*decided == not
			  && strcmp (_NL_CURRENT (LC_TIME, ABMON_1 + cnt),
				     ab_month_name[cnt]))
			*decided = loc;
		      break;
		    }
		}
#endif
	      if (match_string (month_name[cnt], rp)
		  || match_string (ab_month_name[cnt], rp))
		{
		  *decided = raw;
		  break;
		}
	    }
	  if (cnt == 12)
	    /* Does not match a month name.  */
	    return NULL;
	  tm->tm_mon = cnt;
	  want_xday = 1;
	  break;
	case 'c':
	  /* Match locale's date and time format.  */
#ifdef _NL_CURRENT
	  if (*decided != raw)
	    {
	      if (!recursive (_NL_CURRENT (LC_TIME, D_T_FMT)))
		{
		  if (*decided == loc)
		    return NULL;
		  else
		    rp = rp_backup;
		}
	      else
		{
		  if (*decided == not &&
		      strcmp (_NL_CURRENT (LC_TIME, D_T_FMT), HERE_D_T_FMT))
		    *decided = loc;
		  want_xday = 1;
		  break;
		}
	      *decided = raw;
	    }
#endif
	  if (!recursive (HERE_D_T_FMT))
	    return NULL;
	  want_xday = 1;
	  break;
	case 'C':
	  /* Match century number.  */
#ifdef _NL_CURRENT
	match_century:
#endif
	  get_number (0, 99, 2);
	  century = val;
	  want_xday = 1;
	  break;
	case 'd':
	case 'e':
	  /* Match day of month.  */
	  get_number (1, 31, 2);
	  tm->tm_mday = val;
	  have_mday = 1;
	  want_xday = 1;
	  break;
	case 'F':
	  if (!recursive ("%Y-%m-%d"))
	    return NULL;
	  want_xday = 1;
	  break;
	case 'x':
#ifdef _NL_CURRENT
	  if (*decided != raw)
	    {
	      if (!recursive (_NL_CURRENT (LC_TIME, D_FMT)))
		{
		  if (*decided == loc)
		    return NULL;
		  else
		    rp = rp_backup;
		}
	      else
		{
		  if (*decided == not
		      && strcmp (_NL_CURRENT (LC_TIME, D_FMT), HERE_D_FMT))
		    *decided = loc;
		  want_xday = 1;
		  break;
		}
	      *decided = raw;
	    }
#endif
	  /* Fall through.  */
	case 'D':
	  /* Match standard day format.  */
	  if (!recursive (HERE_D_FMT))
	    return NULL;
	  want_xday = 1;
	  break;
	case 'k':
	case 'H':
	  /* Match hour in 24-hour clock.  */
	  get_number (0, 23, 2);
	  tm->tm_hour = val;
	  have_I = 0;
	  break;
	case 'I':
	  /* Match hour in 12-hour clock.  */
	  get_number (1, 12, 2);
	  tm->tm_hour = val % 12;
	  have_I = 1;
	  break;
	case 'j':
	  /* Match day number of year.  */
	  get_number (1, 366, 3);
	  tm->tm_yday = val - 1;
	  have_yday = 1;
	  break;
	case 'm':
	  /* Match number of month.  */
	  get_number (1, 12, 2);
	  tm->tm_mon = val - 1;
	  have_mon = 1;
	  want_xday = 1;
	  break;
	case 'M':
	  /* Match minute.  */
	  get_number (0, 59, 2);
	  tm->tm_min = val;
	  break;
	case 'n':
	case 't':
	  /* Match any white space.  */
	  while (isspace (*rp))
	    ++rp;
	  break;
	case 'p':
	  /* Match locale's equivalent of AM/PM.  */
#ifdef _NL_CURRENT
	  if (*decided != raw)
	    {
	      if (match_string (_NL_CURRENT (LC_TIME, AM_STR), rp))
		{
		  if (strcmp (_NL_CURRENT (LC_TIME, AM_STR), HERE_AM_STR))
		    *decided = loc;
		  break;
		}
	      if (match_string (_NL_CURRENT (LC_TIME, PM_STR), rp))
		{
		  if (strcmp (_NL_CURRENT (LC_TIME, PM_STR), HERE_PM_STR))
		    *decided = loc;
		  is_pm = 1;
		  break;
		}
	      *decided = raw;
	    }
#endif
	  if (!match_string (HERE_AM_STR, rp))
	    if (match_string (HERE_PM_STR, rp))
	      is_pm = 1;
	    else
	      return NULL;
	  break;
	case 'r':
#ifdef _NL_CURRENT
	  if (*decided != raw)
	    {
	      if (!recursive (_NL_CURRENT (LC_TIME, T_FMT_AMPM)))
		{
		  if (*decided == loc)
		    return NULL;
		  else
		    rp = rp_backup;
		}
	      else
		{
		  if (*decided == not &&
		      strcmp (_NL_CURRENT (LC_TIME, T_FMT_AMPM),
			      HERE_T_FMT_AMPM))
		    *decided = loc;
		  break;
		}
	      *decided = raw;
	    }
#endif
	  if (!recursive (HERE_T_FMT_AMPM))
	    return NULL;
	  break;
	case 'R':
	  if (!recursive ("%H:%M"))
	    return NULL;
	  break;
	case 's':
	  {
	    /* The number of seconds may be very high so we cannot use
	       the `get_number' macro.  Instead read the number
	       character for character and construct the result while
	       doing this.  */
	    time_t secs = 0;
	    if (*rp < '0' || *rp > '9')
	      /* We need at least one digit.  */
	      return NULL;

	    do
	      {
		secs *= 10;
		secs += *rp++ - '0';
	      }
	    while (*rp >= '0' && *rp <= '9');

	    if (localtime_r (&secs, tm) == NULL)
	      /* Error in function.  */
	      return NULL;
	  }
	  break;
	case 'S':
	  get_number (0, 61, 2);
	  tm->tm_sec = val;
	  break;
	case 'X':
#ifdef _NL_CURRENT
	  if (*decided != raw)
	    {
	      if (!recursive (_NL_CURRENT (LC_TIME, T_FMT)))
		{
		  if (*decided == loc)
		    return NULL;
		  else
		    rp = rp_backup;
		}
	      else
		{
		  if (strcmp (_NL_CURRENT (LC_TIME, T_FMT), HERE_T_FMT))
		    *decided = loc;
		  break;
		}
	      *decided = raw;
	    }
#endif
	  /* Fall through.  */
	case 'T':
	  if (!recursive (HERE_T_FMT))
	    return NULL;
	  break;
	case 'u':
	  get_number (1, 7, 1);
	  tm->tm_wday = val % 7;
	  have_wday = 1;
	  break;
	case 'g':
	  get_number (0, 99, 2);
	  /* XXX This cannot determine any field in TM.  */
	  break;
	case 'G':
	  if (*rp < '0' || *rp > '9')
	    return NULL;
	  /* XXX Ignore the number since we would need some more
	     information to compute a real date.  */
	  do
	    ++rp;
	  while (*rp >= '0' && *rp <= '9');
	  break;
	case 'U':
	case 'V':
	case 'W':
	  get_number (0, 53, 2);
	  /* XXX This cannot determine any field in TM without some
	     information.  */
	  break;
	case 'w':
	  /* Match number of weekday.  */
	  get_number (0, 6, 1);
	  tm->tm_wday = val;
	  have_wday = 1;
	  break;
	case 'y':
#ifdef _NL_CURRENT
	match_year_in_century:
#endif
	  /* Match year within century.  */
	  get_number (0, 99, 2);
	  /* The "Year 2000: The Millennium Rollover" paper suggests that
	     values in the range 69-99 refer to the twentieth century.  */
	  tm->tm_year = val >= 69 ? val : val + 100;
	  /* Indicate that we want to use the century, if specified.  */
	  want_century = 1;
	  want_xday = 1;
	  break;
	case 'Y':
	  /* Match year including century number.  */
	  get_number (0, 9999, 4);
	  tm->tm_year = val - 1900;
	  want_century = 0;
	  want_xday = 1;
	  break;
	case 'Z':
	  /* XXX How to handle this?  */
	  break;
	case 'E':
#ifdef _NL_CURRENT
	  switch (*fmt++)
	    {
	    case 'c':
	      /* Match locale's alternate date and time format.  */
	      if (*decided != raw)
		{
		  const char *fmt = _NL_CURRENT (LC_TIME, ERA_D_T_FMT);

		  if (*fmt == '\0')
		    fmt = _NL_CURRENT (LC_TIME, D_T_FMT);

		  if (!recursive (fmt))
		    {
		      if (*decided == loc)
			return NULL;
		      else
			rp = rp_backup;
		    }
		  else
		    {
		      if (strcmp (fmt, HERE_D_T_FMT))
			*decided = loc;
		      want_xday = 1;
		      break;
		    }
		  *decided = raw;
		}
	      /* The C locale has no era information, so use the
		 normal representation.  */
	      if (!recursive (HERE_D_T_FMT))
		return NULL;
	      want_xday = 1;
	      break;
	    case 'C':
	      if (*decided != raw)
		{
		  if (era_cnt >= 0)
		    {
		      era = _nl_select_era_entry (era_cnt);
		      if (match_string (era->era_name, rp))
			{
			  *decided = loc;
			  break;
			}
		      else
			return NULL;
		    }
		  else
		    {
		      num_eras = _NL_CURRENT_WORD (LC_TIME,
						   _NL_TIME_ERA_NUM_ENTRIES);
		      for (era_cnt = 0; era_cnt < (int) num_eras;
			   ++era_cnt, rp = rp_backup)
			{
			  era = _nl_select_era_entry (era_cnt);
			  if (match_string (era->era_name, rp))
			    {
			      *decided = loc;
			      break;
			    }
			}
		      if (era_cnt == (int) num_eras)
			{
			  era_cnt = -1;
			  if (*decided == loc)
			    return NULL;
			}
		      else
			break;
		    }

		  *decided = raw;
		}
	      /* The C locale has no era information, so use the
		 normal representation.  */
	      goto match_century;
 	    case 'y':
	      if (*decided == raw)
		goto match_year_in_century;

	      get_number(0, 9999, 4);
	      tm->tm_year = val;
	      want_era = 1;
	      want_xday = 1;
	      break;
	    case 'Y':
	      if (*decided != raw)
		{
		  num_eras = _NL_CURRENT_WORD (LC_TIME,
					       _NL_TIME_ERA_NUM_ENTRIES);
		  for (era_cnt = 0; era_cnt < (int) num_eras;
		       ++era_cnt, rp = rp_backup)
		    {
		      era = _nl_select_era_entry (era_cnt);
		      if (recursive (era->era_format))
			break;
		    }
		  if (era_cnt == (int) num_eras)
		    {
		      era_cnt = -1;
		      if (*decided == loc)
			return NULL;
		      else
			rp = rp_backup;
		    }
		  else
		    {
		      *decided = loc;
		      era_cnt = -1;
		      break;
		    }

		  *decided = raw;
		}
	      get_number (0, 9999, 4);
	      tm->tm_year = val - 1900;
	      want_century = 0;
	      want_xday = 1;
	      break;
	    case 'x':
	      if (*decided != raw)
		{
		  const char *fmt = _NL_CURRENT (LC_TIME, ERA_D_FMT);

		  if (*fmt == '\0')
		    fmt = _NL_CURRENT (LC_TIME, D_FMT);

		  if (!recursive (fmt))
		    {
		      if (*decided == loc)
			return NULL;
		      else
			rp = rp_backup;
		    }
		  else
		    {
		      if (strcmp (fmt, HERE_D_FMT))
			*decided = loc;
		      break;
		    }
		  *decided = raw;
		}
	      if (!recursive (HERE_D_FMT))
		return NULL;
	      break;
	    case 'X':
	      if (*decided != raw)
		{
		  const char *fmt = _NL_CURRENT (LC_TIME, ERA_T_FMT);

		  if (*fmt == '\0')
		    fmt = _NL_CURRENT (LC_TIME, T_FMT);

		  if (!recursive (fmt))
		    {
		      if (*decided == loc)
			return NULL;
		      else
			rp = rp_backup;
		    }
		  else
		    {
		      if (strcmp (fmt, HERE_T_FMT))
			*decided = loc;
		      break;
		    }
		  *decided = raw;
		}
	      if (!recursive (HERE_T_FMT))
		return NULL;
	      break;
	    default:
	      return NULL;
	    }
	  break;
#else
	  /* We have no information about the era format.  Just use
	     the normal format.  */
	  if (*fmt != 'c' && *fmt != 'C' && *fmt != 'y' && *fmt != 'Y'
	      && *fmt != 'x' && *fmt != 'X')
	    /* This is an illegal format.  */
	    return NULL;

	  goto start_over;
#endif
	case 'O':
	  switch (*fmt++)
	    {
	    case 'd':
	    case 'e':
	      /* Match day of month using alternate numeric symbols.  */
	      get_alt_number (1, 31, 2);
	      tm->tm_mday = val;
	      have_mday = 1;
	      want_xday = 1;
	      break;
	    case 'H':
	      /* Match hour in 24-hour clock using alternate numeric
		 symbols.  */
	      get_alt_number (0, 23, 2);
	      tm->tm_hour = val;
	      have_I = 0;
	      break;
	    case 'I':
	      /* Match hour in 12-hour clock using alternate numeric
		 symbols.  */
	      get_alt_number (1, 12, 2);
	      tm->tm_hour = val - 1;
	      have_I = 1;
	      break;
	    case 'm':
	      /* Match month using alternate numeric symbols.  */
	      get_alt_number (1, 12, 2);
	      tm->tm_mon = val - 1;
	      have_mon = 1;
	      want_xday = 1;
	      break;
	    case 'M':
	      /* Match minutes using alternate numeric symbols.  */
	      get_alt_number (0, 59, 2);
	      tm->tm_min = val;
	      break;
	    case 'S':
	      /* Match seconds using alternate numeric symbols.  */
	      get_alt_number (0, 61, 2);
	      tm->tm_sec = val;
	      break;
	    case 'U':
	    case 'V':
	    case 'W':
	      get_alt_number (0, 53, 2);
	      /* XXX This cannot determine any field in TM without
		 further information.  */
	      break;
	    case 'w':
	      /* Match number of weekday using alternate numeric symbols.  */
	      get_alt_number (0, 6, 1);
	      tm->tm_wday = val;
	      have_wday = 1;
	      break;
	    case 'y':
	      /* Match year within century using alternate numeric symbols.  */
	      get_alt_number (0, 99, 2);
	      tm->tm_year = val >= 69 ? val : val + 100;
	      want_xday = 1;
	      break;
	    default:
	      return NULL;
	    }
	  break;
	default:
	  return NULL;
	}
    }

  if (have_I && is_pm)
    tm->tm_hour += 12;

  if (century != -1)
    {
      if (want_century)
	tm->tm_year = tm->tm_year % 100 + (century - 19) * 100;
      else
	/* Only the century, but not the year.  Strange, but so be it.  */
	tm->tm_year = (century - 19) * 100;
    }

#ifdef _NL_CURRENT
  if (era_cnt != -1)
    {
      era = _nl_select_era_entry(era_cnt);
      if (want_era)
	tm->tm_year = (era->start_date[0]
		       + ((tm->tm_year - era->offset)
			  * era->absolute_direction));
      else
	/* Era start year assumed.  */
	tm->tm_year = era->start_date[0];
    }
  else
#endif
    if (want_era)
      return NULL;

  if (want_xday && !have_wday)
    {
      if ( !(have_mon && have_mday) && have_yday)
	{
	  /* We don't have tm_mon and/or tm_mday, compute them.  */
	  int t_mon = 0;
	  while (__mon_yday[__isleap(1900 + tm->tm_year)][t_mon] <= tm->tm_yday)
	      t_mon++;
	  if (!have_mon)
	      tm->tm_mon = t_mon - 1;
	  if (!have_mday)
	      tm->tm_mday =
		(tm->tm_yday
		 - __mon_yday[__isleap(1900 + tm->tm_year)][t_mon - 1] + 1);
	}
      day_of_the_week (tm);
    }
  if (want_xday && !have_yday)
    day_of_the_year (tm);

  return (char *) rp;
}


char *
msn_strptime (buf, format, tm)
     const char *buf;
     const char *format;
     struct tm *tm;
{
  enum locale_status decided;

#ifdef _NL_CURRENT
  decided = not;
#else
  decided = raw;
#endif
  return strptime_internal (buf, format, tm, &decided, -1);
}
#else
#define msn_strptime	strptime
#endif
