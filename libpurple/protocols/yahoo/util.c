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
#endif /* HAVE_CONFIG_H */

#include "debug.h"
#include "internal.h"
#include "prpl.h"

#include "libymsg.h"

#include <string.h>

gboolean
yahoo_account_use_http_proxy(PurpleConnection *conn)
{
	PurpleProxyInfo *ppi = purple_proxy_get_setup(conn->account);
	return (ppi->type == PURPLE_PROXY_HTTP || ppi->type == PURPLE_PROXY_USE_ENVVAR);
}

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
	cookies = ((YahooData*)(gc->proto_data))->cookies;
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
	YahooData *yd = gc->proto_data;
	char *ret;
	const char *to_codeset;

	if (yd->jp)
		return g_strdup(str);

	if (utf8 && *utf8) /* FIXME: maybe don't use utf8 if it'll fit in latin1 */
		return g_strdup(str);

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
	YahooData *yd = gc->proto_data;
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
	const unsigned char *p;

	gstr = g_string_sized_new(strlen(str) * 6 + 1);

	for (p = (unsigned char *)str; *p; p++) {
		g_string_append_printf(gstr, "&#%u;", *p);
	}

	return g_string_free(gstr, FALSE);
}

/*
 * The values in this hash table should probably be lowercase, since that's
 * what xhtml expects.  Also because yahoo_codes_to_html() does
 * case-sensitive comparisons.
 *
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
 * [2;30m <--white out
 */

static GHashTable *esc_codes_ht = NULL;
static GHashTable *tags_ht = NULL;

void yahoo_init_colorht()
{
	if (esc_codes_ht != NULL)
		/* Hash table has already been initialized */
		return;

	/* Key is the escape code string.  Value is the HTML that should be
	 * inserted in place of the escape code. */
	esc_codes_ht = g_hash_table_new(g_str_hash, g_str_equal);

	/* Key is the name of the HTML tag, for example "font" or "/font"
	 * value is the HTML that should be inserted in place of the old tag */
	tags_ht = g_hash_table_new(g_str_hash, g_str_equal);

	/* the numbers in comments are what gyach uses, but i think they're incorrect */
#ifdef USE_CSS_FORMATTING
	g_hash_table_insert(esc_codes_ht, "30", "<span style=\"color: #000000\">"); /* black */
	g_hash_table_insert(esc_codes_ht, "31", "<span style=\"color: #0000FF\">"); /* blue */
	g_hash_table_insert(esc_codes_ht, "32", "<span style=\"color: #008080\">"); /* cyan */      /* 00b2b2 */
	g_hash_table_insert(esc_codes_ht, "33", "<span style=\"color: #808080\">"); /* gray */      /* 808080 */
	g_hash_table_insert(esc_codes_ht, "34", "<span style=\"color: #008000\">"); /* green */     /* 00c200 */
	g_hash_table_insert(esc_codes_ht, "35", "<span style=\"color: #FF0080\">"); /* pink */      /* ffafaf */
	g_hash_table_insert(esc_codes_ht, "36", "<span style=\"color: #800080\">"); /* purple */    /* b200b2 */
	g_hash_table_insert(esc_codes_ht, "37", "<span style=\"color: #FF8000\">"); /* orange */    /* ffff00 */
	g_hash_table_insert(esc_codes_ht, "38", "<span style=\"color: #FF0000\">"); /* red */
	g_hash_table_insert(esc_codes_ht, "39", "<span style=\"color: #808000\">"); /* olive */     /* 546b50 */
#else
	g_hash_table_insert(esc_codes_ht, "30", "<font color=\"#000000\">"); /* black */
	g_hash_table_insert(esc_codes_ht, "31", "<font color=\"#0000FF\">"); /* blue */
	g_hash_table_insert(esc_codes_ht, "32", "<font color=\"#008080\">"); /* cyan */      /* 00b2b2 */
	g_hash_table_insert(esc_codes_ht, "33", "<font color=\"#808080\">"); /* gray */      /* 808080 */
	g_hash_table_insert(esc_codes_ht, "34", "<font color=\"#008000\">"); /* green */     /* 00c200 */
	g_hash_table_insert(esc_codes_ht, "35", "<font color=\"#FF0080\">"); /* pink */      /* ffafaf */
	g_hash_table_insert(esc_codes_ht, "36", "<font color=\"#800080\">"); /* purple */    /* b200b2 */
	g_hash_table_insert(esc_codes_ht, "37", "<font color=\"#FF8000\">"); /* orange */    /* ffff00 */
	g_hash_table_insert(esc_codes_ht, "38", "<font color=\"#FF0000\">"); /* red */
	g_hash_table_insert(esc_codes_ht, "39", "<font color=\"#808000\">"); /* olive */     /* 546b50 */
#endif /* !USE_CSS_FORMATTING */

	g_hash_table_insert(esc_codes_ht,  "1",  "<b>");
	g_hash_table_insert(esc_codes_ht, "x1", "</b>");
	g_hash_table_insert(esc_codes_ht,  "2",  "<i>");
	g_hash_table_insert(esc_codes_ht, "x2", "</i>");
	g_hash_table_insert(esc_codes_ht,  "4",  "<u>");
	g_hash_table_insert(esc_codes_ht, "x4", "</u>");

	/* these just tell us the text they surround is supposed
	 * to be a link. purple figures that out on its own so we
	 * just ignore it.
	 */
	g_hash_table_insert(esc_codes_ht, "l", ""); /* link start */
	g_hash_table_insert(esc_codes_ht, "xl", ""); /* link end */

#ifdef USE_CSS_FORMATTING
	g_hash_table_insert(tags_ht, "black",  "<span style=\"color: #000000\">");
	g_hash_table_insert(tags_ht, "blue",   "<span style=\"color: #0000FF\">");
	g_hash_table_insert(tags_ht, "cyan",   "<span style=\"color: #008284\">");
	g_hash_table_insert(tags_ht, "gray",   "<span style=\"color: #848284\">");
	g_hash_table_insert(tags_ht, "green",  "<span style=\"color: #008200\">");
	g_hash_table_insert(tags_ht, "pink",   "<span style=\"color: #FF0084\">");
	g_hash_table_insert(tags_ht, "purple", "<span style=\"color: #840084\">");
	g_hash_table_insert(tags_ht, "orange", "<span style=\"color: #FF8000\">");
	g_hash_table_insert(tags_ht, "red",    "<span style=\"color: #FF0000\">");
	g_hash_table_insert(tags_ht, "yellow", "<span style=\"color: #848200\">");

	g_hash_table_insert(tags_ht, "/black",  "</span>");
	g_hash_table_insert(tags_ht, "/blue",   "</span>");
	g_hash_table_insert(tags_ht, "/cyan",   "</span>");
	g_hash_table_insert(tags_ht, "/gray",   "</span>");
	g_hash_table_insert(tags_ht, "/green",  "</span>");
	g_hash_table_insert(tags_ht, "/pink",   "</span>");
	g_hash_table_insert(tags_ht, "/purple", "</span>");
	g_hash_table_insert(tags_ht, "/orange", "</span>");
	g_hash_table_insert(tags_ht, "/red",    "</span>");
	g_hash_table_insert(tags_ht, "/yellow", "</span>");
#else
	g_hash_table_insert(tags_ht, "black",  "<font color=\"#000000\">");
	g_hash_table_insert(tags_ht, "blue",   "<font color=\"#0000FF\">");
	g_hash_table_insert(tags_ht, "cyan",   "<font color=\"#008284\">");
	g_hash_table_insert(tags_ht, "gray",   "<font color=\"#848284\">");
	g_hash_table_insert(tags_ht, "green",  "<font color=\"#008200\">");
	g_hash_table_insert(tags_ht, "pink",   "<font color=\"#FF0084\">");
	g_hash_table_insert(tags_ht, "purple", "<font color=\"#840084\">");
	g_hash_table_insert(tags_ht, "orange", "<font color=\"#FF8000\">");
	g_hash_table_insert(tags_ht, "red",    "<font color=\"#FF0000\">");
	g_hash_table_insert(tags_ht, "yellow", "<font color=\"#848200\">");

	g_hash_table_insert(tags_ht, "/black",  "</font>");
	g_hash_table_insert(tags_ht, "/blue",   "</font>");
	g_hash_table_insert(tags_ht, "/cyan",   "</font>");
	g_hash_table_insert(tags_ht, "/gray",   "</font>");
	g_hash_table_insert(tags_ht, "/green",  "</font>");
	g_hash_table_insert(tags_ht, "/pink",   "</font>");
	g_hash_table_insert(tags_ht, "/purple", "</font>");
	g_hash_table_insert(tags_ht, "/orange", "</font>");
	g_hash_table_insert(tags_ht, "/red",    "</font>");
	g_hash_table_insert(tags_ht, "/yellow", "</font>");
#endif /* !USE_CSS_FORMATTING */

	/* We don't support these tags, so discard them */
	g_hash_table_insert(tags_ht, "alt", "");
	g_hash_table_insert(tags_ht, "fade", "");
	g_hash_table_insert(tags_ht, "snd", "");
	g_hash_table_insert(tags_ht, "/alt", "");
	g_hash_table_insert(tags_ht, "/fade", "");

	/* Official clients don't seem to send b, i or u tags.  They use
	 * the escape codes listed above.  Official clients definitely send
	 * font tags, though.  I wonder if we can remove the opening and
	 * closing b, i and u tags from here? */
	g_hash_table_insert(tags_ht, "b", "<b>");
	g_hash_table_insert(tags_ht, "i", "<i>");
	g_hash_table_insert(tags_ht, "u", "<u>");
	g_hash_table_insert(tags_ht, "font", "<font>");

	g_hash_table_insert(tags_ht, "/b", "</b>");
	g_hash_table_insert(tags_ht, "/i", "</i>");
	g_hash_table_insert(tags_ht, "/u", "</u>");
	g_hash_table_insert(tags_ht, "/font", "</font>");
}

void yahoo_dest_colorht()
{
	if (esc_codes_ht == NULL)
		/* Hash table has already been destroyed */
		return;

	g_hash_table_destroy(esc_codes_ht);
	esc_codes_ht = NULL;
	g_hash_table_destroy(tags_ht);
	tags_ht = NULL;
}

#ifndef USE_CSS_FORMATTING
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
#endif /* !USE_CSS_FORMATTING */

static void append_attrs_datalist_foreach_cb(GQuark key_id, gpointer data, gpointer user_data)
{
	const char *key;
	const char *value;
	xmlnode *cur;

	key = g_quark_to_string(key_id);
	value = data;
	cur = user_data;

	xmlnode_set_attrib(cur, key, value);
}

/**
 * @param cur A pointer to the position in the XML tree that we're
 *        currently building.  This will be modified when opening a tag
 *        or closing an existing tag.
 */
static void yahoo_codes_to_html_add_tag(xmlnode **cur, const char *tag, gboolean is_closing_tag, const gchar *tag_name, gboolean is_font_tag)
{
	if (is_closing_tag) {
		xmlnode *tmp;
		GSList *dangling_tags = NULL;

		/* Move up the DOM until we find the opening tag */
		for (tmp = *cur; tmp != NULL; tmp = xmlnode_get_parent(tmp)) {
			/* Add one to tag_name when doing this comparison because it starts with a / */
			if (g_str_equal(tmp->name, tag_name + 1))
				/* Found */
				break;
			dangling_tags = g_slist_prepend(dangling_tags, tmp);
		}
		if (tmp == NULL) {
			/* This is a closing tag with no opening tag.  Useless. */
			purple_debug_error("yahoo", "Ignoring unmatched tag %s", tag);
			g_slist_free(dangling_tags);
			return;
		}

		/* Move our current position up, now that we've closed a tag */
		*cur = xmlnode_get_parent(tmp);

		/* Re-open any tags that were nested below the tag we just closed */
		while (dangling_tags != NULL) {
			tmp = dangling_tags->data;
			dangling_tags = g_slist_delete_link(dangling_tags, dangling_tags);

			/* Create a copy of this tag+attributes (but not child tags or
			 * data) at our new location */
			*cur = xmlnode_new_child(*cur, tmp->name);
			for (tmp = tmp->child; tmp != NULL; tmp = tmp->next)
				if (tmp->type == XMLNODE_TYPE_ATTRIB)
					xmlnode_set_attrib_full(*cur, tmp->name,
							tmp->xmlns, tmp->prefix, tmp->data);
		}
	} else {
		const char *start;
		const char *end;
		GData *attributes;
		char *fontsize = NULL;

		purple_markup_find_tag(tag_name, tag, &start, &end, &attributes);
		*cur = xmlnode_new_child(*cur, tag_name);

		if (is_font_tag) {
			/* Special case for the font size attribute */
			fontsize = g_strdup(g_datalist_get_data(&attributes, "size"));
			if (fontsize != NULL)
				g_datalist_remove_data(&attributes, "size");
		}

		/* Add all font tag attributes */
		g_datalist_foreach(&attributes, append_attrs_datalist_foreach_cb, *cur);
		g_datalist_clear(&attributes);

		if (fontsize != NULL) {
#ifdef USE_CSS_FORMATTING
			/*
			 * The Yahoo font size value is given in pt, even though the HTML
			 * standard for <font size="x"> treats the size as a number on a
			 * scale between 1 and 7.  So we insert the font size as a CSS
			 * style on a span tag.
			 */
			gchar *tmp = g_strdup_printf("font-size: %spt", fontsize);
			*cur = xmlnode_new_child(*cur, "span");
			xmlnode_set_attrib(*cur, "style", tmp);
			g_free(tmp);
#else
			/*
			 * The Yahoo font size value is given in pt, even though the HTML
			 * standard for <font size="x"> treats the size as a number on a
			 * scale between 1 and 7.  So we convert it to an appropriate
			 * value.  This loses precision, which is why CSS formatting is
			 * preferred.  The "absz" attribute remains here for backward
			 * compatibility with UIs that might use it, but it is totally
			 * not standard at all.
			 */
			int size, htmlsize;
			gchar tmp[11];
			size = strtol(fontsize, NULL, 10);
			htmlsize = point_to_html(size);
			sprintf(tmp, "%u", htmlsize);
			xmlnode_set_attrib(*cur, "size", tmp);
			xmlnode_set_attrib(*cur, "absz", fontsize);
#endif /* !USE_CSS_FORMATTING */
			g_free(fontsize);
		}
	}
}

/**
 * Similar to purple_markup_get_tag_name(), but works with closing tags.
 *
 * @return The lowercase name of the tag.  If this is a closing tag then
 *         this value starts with a forward slash.  The caller must free
 *         this string with g_free.
 */
static gchar *yahoo_markup_get_tag_name(const char *tag, gboolean *is_closing_tag)
{
	size_t len;

	*is_closing_tag = (tag[1] == '/');
	if (*is_closing_tag)
		len = strcspn(tag + 1, "> ");
	else
		len = strcspn(tag + 1, "> /");

	return g_utf8_strdown(tag + 1, len);
}

/*
 * Yahoo! messages generally aren't well-formed.  Their markup is
 * more of a flow from start to finish rather than a hierarchy from
 * outer to inner.  They tend to open tags and close them only when
 * necessary.
 *
 * Example: <font size="8">size 8 <font size="16">size 16 <font size="8">size 8 again
 *
 * But we want to send well-formed HTML to the core, so we step through
 * the input string and build an xmlnode tree containing sanitized HTML.
 */
char *yahoo_codes_to_html(const char *x)
{
	size_t x_len;
	xmlnode *html, *cur;
	GString *cdata = g_string_new(NULL);
	int i, j;
	gboolean no_more_gt_brackets = FALSE;
	const char *match;
	gchar *xmlstr1, *xmlstr2;

	x_len = strlen(x);
	html = xmlnode_new("html");

	cur = html;
	for (i = 0; i < x_len; i++) {
		if ((x[i] == 0x1b) && (x[i+1] == '[')) {
			/* This escape sequence signifies the beginning of some
			 * text formatting code */
			j = i + 1;

			while (j++ < x_len) {
				gchar *code;

				if (x[j] != 'm')
					/* Keep looking for the end of this sequence */
					continue;

				/* We've reached the end of the formatting sequence, yay */

				/* Append any character data that belongs in the current node */
				if (cdata->len > 0) {
					xmlnode_insert_data(cur, cdata->str, cdata->len);
					g_string_truncate(cdata, 0);
				}

				code = g_strndup(x + i + 2, j - i - 2);
				if (code[0] == '#') {
#ifdef USE_CSS_FORMATTING
					gchar *tmp = g_strdup_printf("color: %s", code);
					cur = xmlnode_new_child(cur, "span");
					xmlnode_set_attrib(cur, "style", tmp);
					g_free(tmp);
#else
					cur = xmlnode_new_child(cur, "font");
					xmlnode_set_attrib(cur, "color", code);
#endif /* !USE_CSS_FORMATTING */

				} else if ((match = g_hash_table_lookup(esc_codes_ht, code))) {
					/* Some tags are in the hash table only because we
					 * want to ignore them */
					if (match[0] != '\0') {
						gboolean is_closing_tag;
						gchar *tag_name;
						tag_name = yahoo_markup_get_tag_name(match, &is_closing_tag);
						yahoo_codes_to_html_add_tag(&cur, match, is_closing_tag, tag_name, FALSE);
						g_free(tag_name);
					}

				} else {
					purple_debug_error("yahoo",
						"Ignoring unknown ansi code 'ESC[%sm'.\n", code);
				}

				g_free(code);
				i = j;
				break;
			}

		} else if (x[i] == '<' && !no_more_gt_brackets) {
			/* The start of an HTML tag */
			j = i;

			while (j++ < x_len) {
				gchar *tag;
				gboolean is_closing_tag;
				gchar *tag_name;

				if (x[j] != '>') {
					if (x[j] == '"') {
						/* We're inside a quoted attribute value. Skip to the end */
						j++;
						while (j != x_len && x[j] != '"')
							j++;
					} else if (x[j] == '\'') {
						/* We're inside a quoted attribute value. Skip to the end */
						j++;
						while (j != x_len && x[j] != '\'')
							j++;
					}
					if (j != x_len)
						/* Keep looking for the end of this tag */
						continue;

					/* This < has no corresponding > */
					g_string_append_c(cdata, x[i]);
					no_more_gt_brackets = TRUE;
					break;
				}

				tag = g_strndup(x + i, j - i + 1);
				tag_name = yahoo_markup_get_tag_name(tag, &is_closing_tag);

				match = g_hash_table_lookup(tags_ht, tag_name);
				if (match == NULL) {
					/* Unknown tag.  The user probably typed a less-than sign */
					g_string_append_c(cdata, x[i]);
					no_more_gt_brackets = TRUE;
					g_free(tag);
					g_free(tag_name);
					break;
				}

				/* Some tags are in the hash table only because we
				 * want to ignore them */
				if (match[0] != '\0') {
					/* Append any character data that belongs in the current node */
					if (cdata->len > 0) {
						xmlnode_insert_data(cur, cdata->str, cdata->len);
						g_string_truncate(cdata, 0);
					}
					if (g_str_equal(tag_name, "font"))
						/* Font tags are a special case.  We don't
						 * necessarily want to replace the whole thing--
						 * we just want to fix the size attribute. */
						yahoo_codes_to_html_add_tag(&cur, tag, is_closing_tag, tag_name, TRUE);
					else
						yahoo_codes_to_html_add_tag(&cur, match, is_closing_tag, tag_name, FALSE);
				}

				i = j;
				g_free(tag);
				g_free(tag_name);
				break;
			}

		} else {
			g_string_append_c(cdata, x[i]);
		}
	}

	/* Append any remaining character data */
	if (cdata->len > 0)
		xmlnode_insert_data(cur, cdata->str, cdata->len);
	g_string_free(cdata, TRUE);

	/* Serialize our HTML */
	xmlstr1 = xmlnode_to_str(html, NULL);
	xmlnode_free(html);

	/* Strip off the outter HTML node */
	/* This probably isn't necessary, especially if we made the outter HTML
	 * node an empty span.  But the HTML is simpler this way. */
	xmlstr2 = g_strndup(xmlstr1 + 6, strlen(xmlstr1) - 13);
	g_free(xmlstr1);

	purple_debug_misc("yahoo", "yahoo_codes_to_html:  Returning string: '%s'.\n", xmlstr2);
	return xmlstr2;
}

/* borrowed from gtkimhtml */
#define MAX_FONT_SIZE 7
#define POINT_SIZE(x) (_point_sizes [MIN ((x > 0 ? x : 1), MAX_FONT_SIZE) - 1])
static const gint _point_sizes [] = { 8, 10, 12, 14, 20, 30, 40 };

enum fatype
{
	FATYPE_SIZE,
	FATYPE_COLOR,
	FATYPE_FACE,
	FATYPE_JUNK
};

typedef struct
{
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
	if (f->type == FATYPE_COLOR)
		g_free(f->u.color);
	else if (f->type == FATYPE_FACE)
		g_free(f->u.face);
	g_free(f);
}

static void yahoo_htc_list_cleanup(GSList *l)
{
	while (l != NULL) {
		g_free(l->data);
		l = g_slist_delete_link(l, l);
	}
}

static void _parse_font_tag(const char *src, GString *dest, int *i, int *j,
				int len, GSList **colors, GSList **tags, GQueue *ftattr)
{
	int m, n, vstart;
	gboolean quote = FALSE, done = FALSE;

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
						quote = TRUE;
						vstart = n;
						continue;
					} else {
						done = 1;
					}
				}

				if (!quote && ((src[n] == ' ') || (src[n] == '>')))
					done = TRUE;

				if (done) {
					if (!g_ascii_strncasecmp(&src[*j+1], "FACE", m - *j - 1)) {
						fontattr *f;

						f = g_new(fontattr, 1);
						f->type = FATYPE_FACE;
						f->u.face = g_strndup(&src[vstart+1], n-vstart-1);
						if (!ftattr)
							ftattr = g_queue_new();
						g_queue_push_tail(ftattr, f);
						m = n;
						break;
					} else if (!g_ascii_strncasecmp(&src[*j+1], "SIZE", m - *j - 1)) {
						fontattr *f;

						f = g_new(fontattr, 1);
						f->type = FATYPE_SIZE;
						f->u.size = POINT_SIZE(strtol(&src[vstart+1], NULL, 10));
						if (!ftattr)
							ftattr = g_queue_new();
						g_queue_push_tail(ftattr, f);
						m = n;
						break;
					} else if (!g_ascii_strncasecmp(&src[*j+1], "COLOR", m - *j - 1)) {
						fontattr *f;

						f = g_new(fontattr, 1);
						f->type = FATYPE_COLOR;
						f->u.color = g_strndup(&src[vstart+1], n-vstart-1);
						if (!ftattr)
							ftattr = g_queue_new();
						g_queue_push_head(ftattr, f);
						m = n;
						break;
					} else {
						fontattr *f;

						f = g_new(fontattr, 1);
						f->type = FATYPE_JUNK;
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
			gboolean needendtag = FALSE;
			fontattr *f;
			GString *tmp = g_string_new(NULL);

			if (!g_queue_is_empty(ftattr)) {
				while ((f = g_queue_pop_tail(ftattr))) {
					switch (f->type) {
					case FATYPE_SIZE:
						if (!needendtag) {
							needendtag = TRUE;
							g_string_append(dest, "<font ");
						}

						g_string_append_printf(dest, "size=\"%d\" ", f->u.size);
						break;
					case FATYPE_FACE:
						if (!needendtag) {
							needendtag = TRUE;
							g_string_append(dest, "<font ");
						}

						g_string_append_printf(dest, "face=\"%s\" ", f->u.face);
						break;
					case FATYPE_JUNK:
						if (!needendtag) {
							needendtag = TRUE;
							g_string_append(dest, "<font ");
						}

						g_string_append(dest, f->u.junk);
						break;

					case FATYPE_COLOR:
						if (needendtag) {
							g_string_append(tmp, "</font>");
							dest->str[dest->len-1] = '>';
							needendtag = TRUE;
						}

						g_string_append(tmp, *colors ? (*colors)->data : "\033[#000000m");
						g_string_append_printf(dest, "\033[%sm", f->u.color);
						*colors = g_slist_prepend(*colors,
								g_strdup_printf("\033[%sm", f->u.color));
						break;
					}
					fontattr_free(f);
				}

				g_queue_free(ftattr);
				ftattr = NULL;

				if (needendtag) {
					dest->str[dest->len-1] = '>';
					*tags = g_slist_prepend(*tags, g_strdup("</font>"));
					g_string_free(tmp, TRUE);
				} else {
					*tags = g_slist_prepend(*tags, tmp->str);
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
	GSList *colors = NULL;
	GSList *tags = NULL;
	size_t src_len;
	int i, j;
	GString *dest;
	char *esc;
	GQueue *ftattr = NULL;
	gboolean no_more_specials = FALSE;

	src_len = strlen(src);
	dest = g_string_sized_new(src_len);

	for (i = 0; i < src_len; i++) {

		if (src[i] == '<' && !no_more_specials) {
			j = i;

			while (1) {
				j++;

				if (j >= src_len) { /* no '>' */
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
							i = src_len;
							break;
						} else {
							i = t - src;
							break;
						}
					} else if (!g_ascii_strncasecmp(&src[i+1], "A HREF=\"", j - i - 1)) {
						j += 7;
						g_string_append(dest, "\033[lm");
						if (purple_str_has_prefix(src + j, "mailto:"))
							j += sizeof("mailto:") - 1;
						while (1) {
							g_string_append_c(dest, src[j]);
							if (++j >= src_len) {
								i = src_len;
								break;
							}
							if (src[j] == '"') {
								g_string_append(dest, "\033[xlm");
								while (1) {
									if (++j >= src_len) {
										i = src_len;
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
							if (++j >= src_len) {
								g_string_append(dest, &src[i]);
								i = src_len;
								break;
							}
							if (src[j] == '>') {
								i = j;
								break;
							}
						}
					} else if (g_ascii_strncasecmp(&src[i+1], "FONT", j - i - 1)) { /* not interested! */
						while (1) {
							if (++j >= src_len) {
								g_string_append(dest, &src[i]);
								i = src_len;
								break;
							}
							if (src[j] == '>') {
								g_string_append_len(dest, &src[i], j - i + 1);
								i = j;
								break;
							}
						}
					} else { /* yay we have a font tag */
						_parse_font_tag(src, dest, &i, &j, src_len, &colors, &tags, ftattr);
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
						} else if (!g_ascii_strncasecmp(&src[i+1], "/FONT", sublen) && tags != NULL) {
							char *etag;

							etag = tags->data;
							tags = g_slist_delete_link(tags, tags);
							if (etag) {
								g_string_append(dest, etag);
								if (!strcmp(etag, "</font>")) {
									if (colors != NULL) {
										g_free(colors->data);
										colors = g_slist_delete_link(colors, colors);
									}
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
			const char *entity;
			int length;

			entity = purple_markup_unescape_entity(src + i, &length);
			if (entity != NULL) {
				/* src[i] is the start of an HTML entity */
				g_string_append(dest, entity);
				i += length - 1;
			} else
				/* src[i] is a normal character */
				g_string_append_c(dest, src[i]);
		}
	}

	esc = g_strescape(dest->str, NULL);
	purple_debug_misc("yahoo", "yahoo_html_to_codes:  Returning string: '%s'.\n", esc);
	g_free(esc);

	yahoo_htc_list_cleanup(colors);
	yahoo_htc_list_cleanup(tags);

	return g_string_free(dest, FALSE);
}
