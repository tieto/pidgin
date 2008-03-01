/*
 * purple
 *
 * Some code copyright 2003 Tim Ringenbach <omarvo@hotmail.com>
 * (marv on irc.freenode.net)
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "debug.h"
#include "internal.h"
#include "prpl.h"

#include "yahoo.h"

#include <string.h>
/*
 * Returns cookies formatted as a null terminated string for the given connection.
 * Must g_free return value.
 * 
 * TODO:will work, but must test for strict correctness
 */
gchar* yahoo_get_cookies(PurpleConnection *gc)
{
	gchar *ans = NULL;
	gchar *cur;
	char firstflag = 1;
	gchar *t1,*t2,*t3;
	GSList *tmp;
	GSList *cookies;
	cookies = ((struct yahoo_data*)(gc->proto_data))->cookies;
	tmp = cookies;
	while(tmp)
	{
		cur = tmp->data;
		t1 = ans;
		t2 = g_strrstr(cur, ";expires=");
		if(t2 == NULL)
			t2 = g_strrstr(cur, "; expires=");
		if(t2 == NULL)
		{
			if(firstflag)
				ans = g_strdup_printf("%c=%s", cur[0], cur+2);
			else
				ans = g_strdup_printf("%s; %c=%s", t1, cur[0], cur+2);
		}
		else
		{
			t3 = strstr(t2+1, ";");
			if(t3 != NULL)
			{
				t2[0] = '\0';

				if(firstflag)
					ans = g_strdup_printf("%c=%s%s", cur[0], cur+2, t3);
				else
					ans = g_strdup_printf("%s; %c=%s%s", t1, cur[0], cur+2, t3);

				t2[0] = ';';
			}
			else
			{
				t2[0] = '\0';

				if(firstflag)
					ans = g_strdup_printf("%c=%s", cur[0], cur+2);
				else
					ans = g_strdup_printf("%s; %c=%s", t1, cur[0], cur+2);

				t2[0] = ';';
			}
		}
		if(firstflag)
			firstflag = 0;
		else
			g_free(t1);
		tmp = g_slist_next(tmp);
	}
	return ans;
}

/**
 * Encode some text to send to the yahoo server.
 *
 * @param gc The connection handle.
 * @param str The null terminated utf8 string to encode.
 * @param utf8 If not @c NULL, whether utf8 is okay or not.
 *             Even if it is okay, we may not use it. If we
 *             used it, we set this to @c TRUE, else to
 *             @c FALSE. If @c NULL, false is assumed, and
 *             it is not dereferenced.
 * @return The g_malloced string in the appropriate encoding.
 */
char *yahoo_string_encode(PurpleConnection *gc, const char *str, gboolean *utf8)
{
	struct yahoo_data *yd = gc->proto_data;
	char *ret;
	const char *to_codeset;

	if (yd->jp && utf8 && *utf8)
		*utf8 = FALSE;

	if (utf8 && *utf8) /* FIXME: maybe don't use utf8 if it'll fit in latin1 */
		return g_strdup(str);

	if (yd->jp)
		to_codeset = "SHIFT_JIS";
	else
		to_codeset = purple_account_get_string(purple_connection_get_account(gc), "local_charset",  "ISO-8859-1");

	ret = g_convert_with_fallback(str, -1, to_codeset, "UTF-8", "?", NULL, NULL, NULL);
	if (ret)
		return ret;
	else
		return g_strdup("");
}

/**
 * Decode some text received from the server.
 *
 * @param gc The gc handle.
 * @param str The null terminated string to decode.
 * @param utf8 Did the server tell us it was supposed to be utf8?
 * @return The decoded, utf-8 string, which must be g_free()'d.
 */
char *yahoo_string_decode(PurpleConnection *gc, const char *str, gboolean utf8)
{
	struct yahoo_data *yd = gc->proto_data;
	char *ret;
	const char *from_codeset;

	if (utf8) {
		if (g_utf8_validate(str, -1, NULL))
			return g_strdup(str);
	}

	if (yd->jp)
		from_codeset = "SHIFT_JIS";
	else
		from_codeset = purple_account_get_string(purple_connection_get_account(gc), "local_charset",  "ISO-8859-1");

	ret = g_convert_with_fallback(str, -1, "UTF-8", from_codeset, NULL, NULL, NULL, NULL);

	if (ret)
		return ret;
	else
		return g_strdup("");
}

char *yahoo_convert_to_numeric(const char *str)
{
	GString *gstr = NULL;
	char *retstr;
	const unsigned char *p;

	gstr = g_string_sized_new(strlen(str) * 6 + 1);

	for (p = (unsigned char *)str; *p; p++) {
		g_string_append_printf(gstr, "&#%u;", *p);
	}

	retstr = gstr->str;

	g_string_free(gstr, FALSE);

	return retstr;
}

/*
 * I found these on some website but i don't know that they actually
 * work (or are supposed to work). I didn't implement them yet.
 * 
     * [0;30m ---black
     * [1;37m ---white
     * [0;37m ---tan
     * [0;38m ---light black
     * [1;39m ---dark blue
     * [0;32m ---green
     * [0;33m ---yellow
     * [0;35m ---pink
     * [1;35m ---purple
     * [1;30m ---light blue
     * [0;31m ---red
     * [0;34m ---blue
     * [0;36m ---aqua
         * (shift+comma)lyellow(shift+period) ---light yellow
     * (shift+comma)lgreen(shift+period) ---light green
[2;30m <--white out
*/

static GHashTable *ht = NULL;

void yahoo_init_colorht()
{
	ht = g_hash_table_new(g_str_hash, g_str_equal);
/* the numbers in comments are what gyach uses, but i think they're incorrect */
	g_hash_table_insert(ht, "30", "<FONT COLOR=\"#000000\">"); /* black */
	g_hash_table_insert(ht, "31", "<FONT COLOR=\"#0000FF\">"); /* blue */
	g_hash_table_insert(ht, "32", "<FONT COLOR=\"#008080\">"); /* cyan */      /* 00b2b2 */
	g_hash_table_insert(ht, "33", "<FONT COLOR=\"#808080\">"); /* gray */      /* 808080 */
	g_hash_table_insert(ht, "34", "<FONT COLOR=\"#008000\">"); /* green */     /* 00c200 */
	g_hash_table_insert(ht, "35", "<FONT COLOR=\"#FF0080\">"); /* pink */      /* ffafaf */
	g_hash_table_insert(ht, "36", "<FONT COLOR=\"#800080\">"); /* purple */    /* b200b2 */
	g_hash_table_insert(ht, "37", "<FONT COLOR=\"#FF8000\">"); /* orange */    /* ffff00 */
	g_hash_table_insert(ht, "38", "<FONT COLOR=\"#FF0000\">"); /* red */
	g_hash_table_insert(ht, "39", "<FONT COLOR=\"#808000\">"); /* olive */     /* 546b50 */

	g_hash_table_insert(ht,  "1",  "<B>");
	g_hash_table_insert(ht, "x1", "</B>");
	g_hash_table_insert(ht,  "2",  "<I>");
	g_hash_table_insert(ht, "x2", "</I>");
	g_hash_table_insert(ht,  "4",  "<U>");
	g_hash_table_insert(ht, "x4", "</U>");

	/* these just tell us the text they surround is supposed
	 * to be a link. purple figures that out on its own so we
	 * just ignore it.
	 */
	g_hash_table_insert(ht, "l", ""); /* link start */
	g_hash_table_insert(ht, "xl", ""); /* link end */

	g_hash_table_insert(ht, "<black>",  "<FONT COLOR=\"#000000\">");
	g_hash_table_insert(ht, "<blue>",   "<FONT COLOR=\"#0000FF\">");
	g_hash_table_insert(ht, "<cyan>",   "<FONT COLOR=\"#008284\">");
	g_hash_table_insert(ht, "<gray>",   "<FONT COLOR=\"#848284\">");
	g_hash_table_insert(ht, "<green>",  "<FONT COLOR=\"#008200\">");
	g_hash_table_insert(ht, "<pink>",   "<FONT COLOR=\"#FF0084\">");
	g_hash_table_insert(ht, "<purple>", "<FONT COLOR=\"#840084\">");
	g_hash_table_insert(ht, "<orange>", "<FONT COLOR=\"#FF8000\">");
	g_hash_table_insert(ht, "<red>",    "<FONT COLOR=\"#FF0000\">");
	g_hash_table_insert(ht, "<yellow>", "<FONT COLOR=\"#848200\">");

	g_hash_table_insert(ht, "</black>",  "</FONT>");
	g_hash_table_insert(ht, "</blue>",   "</FONT>");
	g_hash_table_insert(ht, "</cyan>",   "</FONT>");
	g_hash_table_insert(ht, "</gray>",   "</FONT>");
	g_hash_table_insert(ht, "</green>",  "</FONT>");
	g_hash_table_insert(ht, "</pink>",   "</FONT>");
	g_hash_table_insert(ht, "</purple>", "</FONT>");
	g_hash_table_insert(ht, "</orange>", "</FONT>");
	g_hash_table_insert(ht, "</red>",    "</FONT>");
	g_hash_table_insert(ht, "</yellow>", "</FONT>");

	/* remove these once we have proper support for <FADE> and <ALT> */
	g_hash_table_insert(ht, "</fade>", "");
	g_hash_table_insert(ht, "</alt>", "");

	/* these are the normal html yahoo sends (besides <font>).
	 * anything else will get turned into &lt;tag&gt;, so if I forgot
	 * about something, please add it. Why Yahoo! has to send unescaped
	 * <'s and >'s that aren't supposed to be html is beyond me.
	 */
	g_hash_table_insert(ht, "<b>", "<b>");
	g_hash_table_insert(ht, "<i>", "<i>");
	g_hash_table_insert(ht, "<u>", "<u>");

	g_hash_table_insert(ht, "</b>", "</b>");
	g_hash_table_insert(ht, "</i>", "</i>");
	g_hash_table_insert(ht, "</u>", "</u>");
	g_hash_table_insert(ht, "</font>", "</font>");
}

void yahoo_dest_colorht()
{
	g_hash_table_destroy(ht);
}

static int point_to_html(int x)
{
	if (x < 9)
		return 1;
	if (x < 11)
		return 2;
	if (x < 13)
		return 3;
	if (x < 17)
		return 4;
	if (x < 25)
		return 5;
	if (x < 35)
		return 6;
	return 7;
}

/* The Yahoo size tag is actually an absz tag; convert it to an HTML size, and include both tags */
static void _font_tags_fix_size(GString *tag, GString *dest)
{
	char *x, *end;
	int size;

	if (((x = strstr(tag->str, "size"))) && ((x = strchr(x, '=')))) {
		while (*x && !g_ascii_isdigit(*x))
			x++;
		if (*x) {
			int htmlsize;

			size = strtol(x, &end, 10);
			htmlsize = point_to_html(size);
			g_string_append_len(dest, tag->str, x - tag->str);
			g_string_append_printf(dest, "%d", htmlsize);
			g_string_append_printf(dest, "\" absz=\"%d", size);
			g_string_append(dest, end);
		} else {
			g_string_append(dest, tag->str);
			return;
		}
	} else {
		g_string_append(dest, tag->str);
		return;
	}
}

char *yahoo_codes_to_html(const char *x)
{
	GString *s, *tmp;
	int i, j, xs, nomoreendtags = 0; /* s/endtags/closinganglebrackets */
	char *match, *ret;

	s = g_string_sized_new(strlen(x));

	for (i = 0, xs = strlen(x); i < xs; i++) {
		if ((x[i] == 0x1b) && (x[i+1] == '[')) {
			j = i + 1;

			while (j++ < xs) {
				if (x[j] != 'm')
					continue;
				else {
					tmp = g_string_new_len(x + i + 2, j - i - 2);
					if (tmp->str[0] == '#')
						g_string_append_printf(s, "<FONT COLOR=\"%s\">", tmp->str);
					else if ((match = (char *) g_hash_table_lookup(ht, tmp->str)))
						g_string_append(s, match);
					else {
						purple_debug(PURPLE_DEBUG_ERROR, "yahoo",
							"Unknown ansi code 'ESC[%sm'.\n", tmp->str);
						g_string_free(tmp, TRUE);
						break;
					}

					i = j;
					g_string_free(tmp, TRUE);
					break;
				}
			}

		} else if (!nomoreendtags && (x[i] == '<')) {
			j = i;

			while (j++ < xs) {
				if (x[j] != '>')
					if (j == xs) {
						g_string_append(s, "&lt;");
						nomoreendtags = 1;
					}
					else
						continue;
				else {
					tmp = g_string_new_len(x + i, j - i + 1);
					g_string_ascii_down(tmp);

					if ((match = (char *) g_hash_table_lookup(ht, tmp->str)))
						g_string_append(s, match);
					else if (!strncmp(tmp->str, "<fade ", 6) ||
						!strncmp(tmp->str, "<alt ", 5) ||
						!strncmp(tmp->str, "<snd ", 5)) {

						/* remove this if gtkimhtml ever supports any of these */
						i = j;
						g_string_free(tmp, TRUE);
						break;

					} else if (!strncmp(tmp->str, "<font ", 6)) {
						_font_tags_fix_size(tmp, s);
					} else {
						g_string_append(s, "&lt;");
						g_string_free(tmp, TRUE);
						break;
					}

					i = j;
					g_string_free(tmp, TRUE);
					break;
				}

			}

		} else {
			if (x[i] == '<')
				g_string_append(s, "&lt;");
			else if (x[i] == '>')
				g_string_append(s, "&gt;");
			else if (x[i] == '&')
				g_string_append(s, "&amp;");
			else if (x[i] == '"')
				g_string_append(s, "&quot;");
			else
				g_string_append_c(s, x[i]);
		}
	}

	ret = s->str;
	g_string_free(s, FALSE);
	purple_debug(PURPLE_DEBUG_MISC, "yahoo", "yahoo_codes_to_html:  Returning string: '%s'.\n", ret);
	return ret;
}

/* borrowed from gtkimhtml */
#define MAX_FONT_SIZE 7
#define POINT_SIZE(x) (_point_sizes [MIN ((x > 0 ? x : 1), MAX_FONT_SIZE) - 1])
static const gint _point_sizes [] = { 8, 10, 12, 14, 20, 30, 40 };

enum fatype { size, color, face, junk };
typedef struct {
	enum fatype type;
	union {
		int size;
		char *color;
		char *face;
		char *junk;
	} u;
} fontattr;

static void fontattr_free(fontattr *f)
{
	if (f->type == color)
		g_free(f->u.color);
	else if (f->type == face)
		g_free(f->u.face);
	g_free(f);
}

static void yahoo_htc_queue_cleanup(GQueue *q)
{
	char *tmp;

	while ((tmp = g_queue_pop_tail(q)))
		g_free(tmp);
	g_queue_free(q);
}

static void _parse_font_tag(const char *src, GString *dest, int *i, int *j,
				int len, GQueue *colors, GQueue *tags, GQueue *ftattr)
{

	int m, n, vstart;
	gboolean quote = 0, done = 0;

	m = *j;

	while (1) {
		m++;

		if (m >= len) {
			g_string_append(dest, &src[*i]);
			*i = len;
			break;
		}

		if (src[m] == '=') {
			n = vstart = m;
			while (1) {
				n++;

				if (n >= len) {
					m = n;
					break;
				}

				if (src[n] == '"') {
					if (!quote) {
						quote = 1;
						vstart = n;
						continue;
					} else {
						done = 1;
					}
				}

				if (!quote && ((src[n] == ' ') || (src[n] == '>')))
					done = 1;

				if (done) {
					if (!g_ascii_strncasecmp(&src[*j+1], "FACE", m - *j - 1)) {
						fontattr *f;

						f = g_new(fontattr, 1);
						f->type = face;
						f->u.face = g_strndup(&src[vstart+1], n-vstart-1);
						if (!ftattr)
							ftattr = g_queue_new();
						g_queue_push_tail(ftattr, f);
						m = n;
						break;
					} else if (!g_ascii_strncasecmp(&src[*j+1], "SIZE", m - *j - 1)) {
						fontattr *f;

						f = g_new(fontattr, 1);
						f->type = size;
						f->u.size = POINT_SIZE(strtol(&src[vstart+1], NULL, 10));
						if (!ftattr)
							ftattr = g_queue_new();
						g_queue_push_tail(ftattr, f);
						m = n;
						break;
					} else if (!g_ascii_strncasecmp(&src[*j+1], "COLOR", m - *j - 1)) {
						fontattr *f;

						f = g_new(fontattr, 1);
						f->type = color;
						f->u.color = g_strndup(&src[vstart+1], n-vstart-1);
						if (!ftattr)
							ftattr = g_queue_new();
						g_queue_push_head(ftattr, f);
						m = n;
						break;
					} else {
						fontattr *f;

						f = g_new(fontattr, 1);
						f->type = junk;
						f->u.junk = g_strndup(&src[*j+1], n-*j);
						if (!ftattr)
							ftattr = g_queue_new();
						g_queue_push_tail(ftattr, f);
						m = n;
						break;
					}

				}
			}
		}

		if (src[m] == ' ')
			*j = m;

		if (src[m] == '>') {
			gboolean needendtag = 0;
			fontattr *f;
			GString *tmp = g_string_new(NULL);
			char *colorstr;

			if (!g_queue_is_empty(ftattr)) {
				while ((f = g_queue_pop_tail(ftattr))) {
					switch (f->type) {
					case size:
						if (!needendtag) {
							needendtag = 1;
							g_string_append(dest, "<font ");
						}

						g_string_append_printf(dest, "size=\"%d\" ", f->u.size);
						fontattr_free(f);
						break;
					case face:
						if (!needendtag) {
							needendtag = 1;
							g_string_append(dest, "<font ");
						}

						g_string_append_printf(dest, "face=\"%s\" ", f->u.face);
						fontattr_free(f);
						break;
					case junk:
						if (!needendtag) {
							needendtag = 1;
							g_string_append(dest, "<font ");
						}

						g_string_append(dest, f->u.junk);
						fontattr_free(f);
						break;

					case color:
						if (needendtag) {
							g_string_append(tmp, "</font>");
							dest->str[dest->len-1] = '>';
							needendtag = 0;
						}

						colorstr = g_queue_peek_tail(colors);
						g_string_append(tmp, colorstr ? colorstr : "\033[#000000m");
						g_string_append_printf(dest, "\033[%sm", f->u.color);
						g_queue_push_tail(colors, g_strdup_printf("\033[%sm", f->u.color));
						fontattr_free(f);
						break;
					}
				}

				g_queue_free(ftattr);
				ftattr = NULL;

				if (needendtag) {
					dest->str[dest->len-1] = '>';
					g_queue_push_tail(tags, g_strdup("</font>"));
					g_string_free(tmp, TRUE);
				} else {
					g_queue_push_tail(tags, tmp->str);
					g_string_free(tmp, FALSE);
				}
			}

			*i = *j = m;
			break;
		}
	}

}

char *yahoo_html_to_codes(const char *src)
{
	int i, j, len;
	GString *dest;
	char *ret, *esc;
	GQueue *colors, *tags;
	GQueue *ftattr = NULL;
	gboolean no_more_specials = FALSE;

	colors = g_queue_new();
	tags = g_queue_new();
	dest = g_string_sized_new(strlen(src));

	for (i = 0, len = strlen(src); i < len; i++) {

		if (!no_more_specials && src[i] == '<') {
			j = i;

			while (1) {
				j++;

				if (j >= len) { /* no '>' */
					g_string_append_c(dest, src[i]);
					no_more_specials = TRUE;
					break;
				}

				if (src[j] == '<') {
					/* FIXME: This doesn't convert outgoing entities.
					 *        However, I suspect this case may never
					 *        happen anymore because of the entities.
					 */
					g_string_append_len(dest, &src[i], j - i);
					i = j - 1;
					if (ftattr) {
						fontattr *f;

						while ((f = g_queue_pop_head(ftattr)))
							fontattr_free(f);
						g_queue_free(ftattr);
						ftattr = NULL;
					}
					break;
				}

				if (src[j] == ' ') {
					if (!g_ascii_strncasecmp(&src[i+1], "BODY", j - i - 1)) {
						char *t = strchr(&src[j], '>');
						if (!t) {
							g_string_append(dest, &src[i]);
							i = len;
							break;
						} else {
							i = t - src;
							break;
						}
					} else if (!g_ascii_strncasecmp(&src[i+1], "A HREF=\"", j - i - 1)) {
						j += 7;
						g_string_append(dest, "\033[lm");
						while (1) {
							g_string_append_c(dest, src[j]);
							if (++j >= len) {
								i = len;
								break;
							}
							if (src[j] == '"') {
								g_string_append(dest, "\033[xlm");
								while (1) {
									if (++j >= len) {
										i = len;
										break;
									}
									if (!g_ascii_strncasecmp(&src[j], "</A>", 4)) {
										j += 3;
										break;
									}
								}
								i = j;
								break;
							}
						}
					} else if (!g_ascii_strncasecmp(&src[i+1], "SPAN", j - i - 1)) { /* drop span tags */
						while (1) {
							if (++j >= len) {
								g_string_append(dest, &src[i]);
								i = len;
								break;
							}
							if (src[j] == '>') {
								i = j;
								break;
							}
						}
					} else if (g_ascii_strncasecmp(&src[i+1], "FONT", j - i - 1)) { /* not interested! */
						while (1) {
							if (++j >= len) {
								g_string_append(dest, &src[i]);
								i = len;
								break;
							}
							if (src[j] == '>') {
								g_string_append_len(dest, &src[i], j - i + 1);
								i = j;
								break;
							}
						}
					} else { /* yay we have a font tag */
						_parse_font_tag(src, dest, &i, &j, len, colors, tags, ftattr);
					}

					break;
				}

				if (src[j] == '>') {
					/* This has some problems like the FIXME for the
					 * '<' case. and like that case, I suspect the case
					 * that this has problems is won't happen anymore anyway.
					 */
					int sublen = j - i - 1;

					if (sublen) {
						if (!g_ascii_strncasecmp(&src[i+1], "B", sublen)) {
							g_string_append(dest, "\033[1m");
						} else if (!g_ascii_strncasecmp(&src[i+1], "/B", sublen)) {
							g_string_append(dest, "\033[x1m");
						} else if (!g_ascii_strncasecmp(&src[i+1], "I", sublen)) {
							g_string_append(dest, "\033[2m");
						} else if (!g_ascii_strncasecmp(&src[i+1], "/I", sublen)) {
							g_string_append(dest, "\033[x2m");
						} else if (!g_ascii_strncasecmp(&src[i+1], "U", sublen)) {
							g_string_append(dest, "\033[4m");
						} else if (!g_ascii_strncasecmp(&src[i+1], "/U", sublen)) {
							g_string_append(dest, "\033[x4m");
						} else if (!g_ascii_strncasecmp(&src[i+1], "/A", sublen)) {
							g_string_append(dest, "\033[xlm");
						} else if (!g_ascii_strncasecmp(&src[i+1], "BR", sublen)) {
							g_string_append_c(dest, '\n');
						} else if (!g_ascii_strncasecmp(&src[i+1], "/BODY", sublen)) {
							/* mmm, </body> tags. *BURP* */
						} else if (!g_ascii_strncasecmp(&src[i+1], "/SPAN", sublen)) {
							/* </span> tags. dangerously close to </spam> */
						} else if (!g_ascii_strncasecmp(&src[i+1], "/FONT", sublen) && g_queue_peek_tail(tags)) {
							char *etag, *cl;

							etag = g_queue_pop_tail(tags);
							if (etag) {
								g_string_append(dest, etag);
								if (!strcmp(etag, "</font>")) {
									cl = g_queue_pop_tail(colors);
									if (cl)
										g_free(cl);
								}
								g_free(etag);
							}
						} else {
							g_string_append_len(dest, &src[i], j - i + 1);
						}
					} else {
						g_string_append_len(dest, &src[i], j - i + 1);
					}

					i = j;
					break;
				}

			}

		} else {
			if (((len - i) >= 4) && !strncmp(&src[i], "&lt;", 4)) {
				g_string_append_c(dest, '<');
				i += 3;
			} else if (((len - i) >= 4) && !strncmp(&src[i], "&gt;", 4)) {
				g_string_append_c(dest, '>');
				i += 3;
			} else if (((len - i) >= 5) && !strncmp(&src[i], "&amp;", 5)) {
				g_string_append_c(dest, '&');
				i += 4;
			} else if (((len - i) >= 6) && !strncmp(&src[i], "&quot;", 6)) {
				g_string_append_c(dest, '"');
				i += 5;
			} else if (((len - i) >= 6) && !strncmp(&src[i], "&apos;", 6)) {
				g_string_append_c(dest, '\'');
				i += 5;
			} else {
				g_string_append_c(dest, src[i]);
			}
		}
	}

	ret = dest->str;
	g_string_free(dest, FALSE);

	esc = g_strescape(ret, NULL);
	purple_debug(PURPLE_DEBUG_MISC, "yahoo", "yahoo_html_to_codes:  Returning string: '%s'.\n", esc);
	g_free(esc);

	yahoo_htc_queue_cleanup(colors);
	yahoo_htc_queue_cleanup(tags);

	return ret;
}
