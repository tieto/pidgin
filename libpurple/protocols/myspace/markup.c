/* MySpaceIM Protocol Plugin - markup
 *
 * Copyright (C) 2007, Jeff Connelly <jeff2@soc.pidgin.im>
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

#include "myspace.h"

typedef int (*MSIM_XMLNODE_CONVERT)(MsimSession *, xmlnode *, gchar **, gchar **);

/* Internal functions */

static guint msim_point_to_purple_size(MsimSession *session, guint point);
static guint msim_purple_size_to_point(MsimSession *session, guint size);
static guint msim_height_to_point(MsimSession *session, guint height);
static guint msim_point_to_height(MsimSession *session, guint point);

static int msim_markup_tag_to_html(MsimSession *, xmlnode *root, gchar **begin, gchar **end);
static int html_tag_to_msim_markup(MsimSession *, xmlnode *root, gchar **begin, gchar **end);
static gchar *msim_convert_xml(MsimSession *, const gchar *raw, MSIM_XMLNODE_CONVERT f);
static gchar *msim_convert_smileys_to_markup(gchar *before);
static double msim_round(double round);


/* Globals */

/* The names in in emoticon_names (for <i n=whatever>) map to corresponding 
 * entries in emoticon_symbols (for the ASCII representation of the emoticon).
 *
 * Multiple emoticon symbols in Pidgin can map to one name. List the
 * canonical form, as inserted by the "Smile!" dialog, first. For example,
 * :) comes before :-), because although both are recognized as 'happy',
 * the first is inserted by the smiley button (first symbol in theme).
 *
 * Note that symbols are case-sensitive in Pidgin -- :-X is not :-x. */
static struct MSIM_EMOTICON
{
	gchar *name;
	gchar *symbol;
} msim_emoticons[] = {
	/* Unfortunately, this list duplicates much of the file
	 * pidgin/pidgin/pixmaps/emotes/default/22/default.theme.in, because
	 * that file is part of Pidgin, but we're part of libpurple.
	 */
	{ "bigsmile", ":D" },
	{ "bigsmile", ":-D" },
	{ "devil", "}:)" },
	{ "frazzled", ":Z" },
	{ "geek", "B)" },
	{ "googles", "%)" },
	{ "growl", ":E" },
	{ "laugh", ":))" },		/* Must be before ':)' */
	{ "happy", ":)" },
	{ "happy", ":-)" },
	{ "happi", ":)" },
	{ "heart", ":X" },
	{ "mohawk", "-:" },
	{ "mad", "X(" },
	{ "messed", "X)" },
	{ "nerd", "Q)" },
	{ "oops", ":G" },
	{ "pirate", "P)" },
	{ "scared", ":O" },
	{ "sidefrown", ":{" },
	{ "sinister", ":B" },
	{ "smirk", ":," },
	{ "straight", ":|" },
	{ "tongue", ":P" },
	{ "tongue", ":p" },
	{ "tongy", ":P" },
	{ "upset", "B|" },
	{ "wink", ";-)" },
	{ "wink", ";)" },
	{ "winc", ";)" },
	{ "worried", ":[" },
	{ "kiss", ":x" },
	{ NULL, NULL }
};



/* Indexes of this array + 1 map HTML font size to scale of normal font size. *
 * Based on _point_sizes from libpurple/gtkimhtml.c 
 *                                 1    2  3    4     5      6       7 */
static gdouble _font_scale[] = { .85, .95, 1, 1.2, 1.44, 1.728, 2.0736 };

#define MAX_FONT_SIZE                   7       /* Purple maximum font size */
#define POINTS_PER_INCH                 72      /* How many pt's in an inch */

/* Text formatting bits for <f s=#> */
#define MSIM_TEXT_BOLD                  1
#define MSIM_TEXT_ITALIC                2   
#define MSIM_TEXT_UNDERLINE             4

/* Default baseline size of purple's fonts, in points. What is size 3 in points. 
 * _font_scale specifies scaling factor relative to this point size. Note this 
 * is only the default; it is configurable in account options. */
#define MSIM_BASE_FONT_POINT_SIZE       8

/* Default display's DPI. 96 is common but it can differ. Also configurable
 * in account options. */
#define MSIM_DEFAULT_DPI                96


/* round is part of C99, but sometimes is unavailable before then.
 * Based on http://forums.belution.com/en/cpp/000/050/13.shtml
 */
double msim_round(double value)
{
	if (value < 0) {
		return -(floor(-value + 0.5));
	} else {
		return   floor( value + 0.5);
	}
}


/** Convert typographical font point size to HTML font size. 
 * Based on libpurple/gtkimhtml.c */
static guint
msim_point_to_purple_size(MsimSession *session, guint point)
{
	guint size, this_point, base;
	gdouble scale;
	
	base = purple_account_get_int(session->account, "base_font_size", MSIM_BASE_FONT_POINT_SIZE);
   
	for (size = 0; 
			size < sizeof(_font_scale) / sizeof(_font_scale[0]);
			++size) {
		scale = _font_scale[CLAMP(size, 1, MAX_FONT_SIZE) - 1];
		this_point = (guint)msim_round(scale * base);

		if (this_point >= point) {
			purple_debug_info("msim", "msim_point_to_purple_size: %d pt -> size=%d\n",
					point, size);
			return size;
		}
	}

	/* No HTML font size was this big; return largest possible. */
	return this_point;
}

/** Convert HTML font size to point size. */
static guint
msim_purple_size_to_point(MsimSession *session, guint size)
{
	gdouble scale;
	guint point;
	guint base;

	scale = _font_scale[CLAMP(size, 1, MAX_FONT_SIZE) - 1];

	base = purple_account_get_int(session->account, "base_font_size", MSIM_BASE_FONT_POINT_SIZE);

	point = (guint)msim_round(scale * base);

	purple_debug_info("msim", "msim_purple_size_to_point: size=%d -> %d pt\n",
					size, point);

	return point;
}

/** Convert a msim markup font pixel height to the more usual point size, for incoming messages. */
static guint 
msim_height_to_point(MsimSession *session, guint height)
{
	guint dpi;

	dpi = purple_account_get_int(session->account, "dpi", MSIM_DEFAULT_DPI);

	return (guint)msim_round((POINTS_PER_INCH * 1. / dpi) * height);

	/* See also: libpurple/protocols/bonjour/jabber.c
	 * _font_size_ichat_to_purple */
}

/** Convert point size to msim pixel height font size specification, for outgoing messages. */
static guint
msim_point_to_height(MsimSession *session, guint point)
{
	guint dpi;

	dpi = purple_account_get_int(session->account, "dpi", MSIM_DEFAULT_DPI);

	return (guint)msim_round((dpi * 1. / POINTS_PER_INCH) * point);
}

/** Convert the msim markup <f> (font) tag into HTML. */
static void 
msim_markup_f_to_html(MsimSession *session, xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *face, *height_str, *decor_str;
	GString *gs_end, *gs_begin;
	guint decor, height;

	face = xmlnode_get_attrib(root, "f");
	height_str = xmlnode_get_attrib(root, "h");
	decor_str = xmlnode_get_attrib(root, "s");

	if (height_str) {
		height = atol(height_str);
	} else {
		height = 12;
	}

	if (decor_str) {
		decor = atol(decor_str);
	} else {
		decor = 0;
	}

	gs_begin = g_string_new("");
	/* TODO: get font size working */
	if (height && !face) {
		g_string_printf(gs_begin, "<font size='%d'>", 
				msim_point_to_purple_size(session, msim_height_to_point(session, height)));
	} else if (height && face) {
		g_string_printf(gs_begin, "<font face='%s' size='%d'>", face,  
				msim_point_to_purple_size(session, msim_height_to_point(session, height)));
	} else {
		g_string_printf(gs_begin, "<font>");
	}

	/* No support for font-size CSS? */
	/* g_string_printf(gs_begin, "<span style='font-family: %s; font-size: %dpt'>", face, 
			msim_height_to_point(height)); */

	gs_end = g_string_new("</font>");

	if (decor & MSIM_TEXT_BOLD) {
		g_string_append(gs_begin, "<b>");
		g_string_prepend(gs_end, "</b>");
	}

	if (decor & MSIM_TEXT_ITALIC) {
		g_string_append(gs_begin, "<i>");
		g_string_append(gs_end, "</i>");
	}

	if (decor & MSIM_TEXT_UNDERLINE) {
		g_string_append(gs_begin, "<u>");
		g_string_append(gs_end, "</u>");
	}


	*begin = g_string_free(gs_begin, FALSE);
	*end = g_string_free(gs_end, FALSE);
}

/** Convert a msim markup color to a color suitable for libpurple.
  *
  * @param msim Either a color name, or an rgb(x,y,z) code.
  *
  * @return A new string, either a color name or #rrggbb code. Must g_free(). 
  */
static char *
msim_color_to_purple(const char *msim)
{
	guint red, green, blue;

	if (!msim) {
		return g_strdup("black");
	}

	if (sscanf(msim, "rgb(%d,%d,%d)", &red, &green, &blue) != 3) {
		/* Color name. */
		return g_strdup(msim);
	}
	/* TODO: rgba (alpha). */

	return g_strdup_printf("#%.2x%.2x%.2x", red, green, blue);
}

/** Convert the msim markup <a> (anchor) tag into HTML. */
static void 
msim_markup_a_to_html(MsimSession *session, xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *href;

	href = xmlnode_get_attrib(root, "h");
	if (!href) {
		href = "";
	}

	*begin = g_strdup_printf("<a href=\"%s\">%s", href, href);
	*end = g_strdup("</a>");
}

/** Convert the msim markup <p> (paragraph) tag into HTML. */
static void 
msim_markup_p_to_html(MsimSession *session, xmlnode *root, gchar **begin, gchar **end)
{
	/* Just pass through unchanged. 
	 *
	 * Note: attributes currently aren't passed, if there are any. */
	*begin = g_strdup("<p>");
	*end = g_strdup("</p>");
}

/** Convert the msim markup <c> tag (text color) into HTML. TODO: Test */
static void 
msim_markup_c_to_html(MsimSession *session, xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *color;
	gchar *purple_color;

	color = xmlnode_get_attrib(root, "v");
	if (!color) {
		purple_debug_info("msim", "msim_markup_c_to_html: <c> tag w/o v attr\n");
		*begin = g_strdup("");
		*end = g_strdup("");
		/* TODO: log as unrecognized */
		return;
	}

	purple_color = msim_color_to_purple(color);

	*begin = g_strdup_printf("<font color='%s'>", purple_color); 

	g_free(purple_color);

	/* *begin = g_strdup_printf("<span style='color: %s'>", color); */
	*end = g_strdup("</font>");
}

/** Convert the msim markup <b> tag (background color) into HTML. TODO: Test */
static void 
msim_markup_b_to_html(MsimSession *session, xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *color;
	gchar *purple_color;

	color = xmlnode_get_attrib(root, "v");
	if (!color) {
		*begin = g_strdup("");
		*end = g_strdup("");
		purple_debug_info("msim", "msim_markup_b_to_html: <b> w/o v attr\n");
		/* TODO: log as unrecognized. */
		return;
	}

	purple_color = msim_color_to_purple(color);

	/* TODO: find out how to set background color. */
	*begin = g_strdup_printf("<span style='background-color: %s'>", 
			purple_color);
	g_free(purple_color);

	*end = g_strdup("</p>");
}

/** Convert the msim markup <i> tag (emoticon image) into HTML. */
static void 
msim_markup_i_to_html(MsimSession *session, xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *name;
	guint i;
	struct MSIM_EMOTICON *emote;

	name = xmlnode_get_attrib(root, "n");
	if (!name) {
		purple_debug_info("msim", "msim_markup_i_to_html: <i> w/o n\n");
		*begin = g_strdup("");
		*end = g_strdup("");
		/* TODO: log as unrecognized */
		return;
	}

	/* Find and use canonical form of smiley symbol. */
	for (i = 0; (emote = &msim_emoticons[i]) && emote->name != NULL; ++i) {
		if (g_str_equal(name, emote->name)) {
			*begin = g_strdup(emote->symbol);
			*end = g_strdup("");
			return;
		}
	}

	/* Couldn't find it, sorry. Try to degrade gracefully. */
	*begin = g_strdup_printf("**%s**", name);
	*end = g_strdup("");
}

/** Convert an individual msim markup tag to HTML. */
static int 
msim_markup_tag_to_html(MsimSession *session, xmlnode *root, gchar **begin, 
		gchar **end)
{
	g_return_val_if_fail(root != NULL, 0);

	if (g_str_equal(root->name, "f")) {
		msim_markup_f_to_html(session, root, begin, end);
	} else if (g_str_equal(root->name, "a")) {
		msim_markup_a_to_html(session, root, begin, end);
	} else if (g_str_equal(root->name, "p")) {
		msim_markup_p_to_html(session, root, begin, end);
	} else if (g_str_equal(root->name, "c")) {
		msim_markup_c_to_html(session, root, begin, end);
	} else if (g_str_equal(root->name, "b")) {
		msim_markup_b_to_html(session, root, begin, end);
	} else if (g_str_equal(root->name, "i")) {
		msim_markup_i_to_html(session, root, begin, end);
	} else {
		purple_debug_info("msim", "msim_markup_tag_to_html: "
				"unknown tag name=%s, ignoring", 
				root->name ? root->name : "(NULL)");
		*begin = g_strdup("");
		*end = g_strdup("");
	}
	return 0;
}

/** Convert an individual HTML tag to msim markup. */
static int 
html_tag_to_msim_markup(MsimSession *session, xmlnode *root, gchar **begin, 
		gchar **end)
{
	int ret = 0;

	if (!purple_utf8_strcasecmp(root->name, "root") ||
	    !purple_utf8_strcasecmp(root->name, "html")) {
		*begin = g_strdup("");
		*end = g_strdup("");
	/* TODO: Coalesce nested tags into one <f> tag!
	 * Currently, the 's' value will be overwritten when b/i/u is nested
	 * within another one, and only the inner-most formatting will be 
	 * applied to the text. */
	} else if (!purple_utf8_strcasecmp(root->name, "b")) {
		if (root->child->type == XMLNODE_TYPE_DATA) {
			*begin = g_strdup_printf("<f s='%d'>", MSIM_TEXT_BOLD);
			*end = g_strdup("</f>");
		} else {
			if (!purple_utf8_strcasecmp(root->child->name,"i")) {
				ret++;
				if (root->child->child->type == XMLNODE_TYPE_DATA) {
					*begin = g_strdup_printf("<f s='%d'>", (MSIM_TEXT_BOLD + MSIM_TEXT_ITALIC));
					*end = g_strdup("</f>");
				} else {
					if (!purple_utf8_strcasecmp(root->child->child->name,"u")) {
						ret++;
						*begin = g_strdup_printf("<f s='%d'>", (MSIM_TEXT_BOLD + MSIM_TEXT_ITALIC + MSIM_TEXT_UNDERLINE));
						*end = g_strdup("</f>");
					}
				}
			} else if (!purple_utf8_strcasecmp(root->child->name,"u")) {
				ret++;
				*begin = g_strdup_printf("<f s='%d'>", (MSIM_TEXT_BOLD + MSIM_TEXT_UNDERLINE));
				*end = g_strdup("</f>");
			}
		}
	} else if (!purple_utf8_strcasecmp(root->name, "i")) {
		if (root->child->type == XMLNODE_TYPE_DATA) {
			*begin = g_strdup_printf("<f s='%d'>", MSIM_TEXT_ITALIC);
			*end = g_strdup("</f>");
		} else {
			if (!purple_utf8_strcasecmp(root->child->name,"u")) {
				ret++;
				*begin = g_strdup_printf("<f s='%d'>", (MSIM_TEXT_ITALIC + MSIM_TEXT_UNDERLINE));
				*end = g_strdup("</f>");
			}
		}
	} else if (!purple_utf8_strcasecmp(root->name, "u")) {
		*begin = g_strdup_printf("<f s='%d'>", MSIM_TEXT_UNDERLINE);
		*end = g_strdup("</f>");
	} else if (!purple_utf8_strcasecmp(root->name, "a")) {
		const gchar *href;
		gchar *link_text;

		href = xmlnode_get_attrib(root, "href");

		if (!href) {
			href = xmlnode_get_attrib(root, "HREF");
		}

		link_text = xmlnode_get_data(root);

		if (href) {
			if (g_str_equal(link_text, href)) {
				/* Purple gives us: <a href="URL">URL</a>
				 * Translate to <a h='URL' />
				 * Displayed as text of URL with link to URL
				 */
				*begin = g_strdup_printf("<a h='%s' />", href);
			} else {
				/* But if we get: <a href="URL">text</a>
				 * Translate to: text: <a h='URL' />
				 *
				 * Because official client only supports self-closed <a>
				 * tags; you can't change the link text.
				 */
				*begin = g_strdup_printf("%s: <a h='%s' />", link_text, href);
			}
		} else {
			*begin = g_strdup("<a />");
		}

		/* Sorry, kid. MySpace doesn't support you within <a> tags. */
		xmlnode_free(root->child);
		g_free(link_text);
		root->child = NULL;

		*end = g_strdup("");
	} else if (!purple_utf8_strcasecmp(root->name, "font")) {
		const gchar *size;
		const gchar *face;

		size = xmlnode_get_attrib(root, "size");
		face = xmlnode_get_attrib(root, "face");

		if (face && size) {
			*begin = g_strdup_printf("<f f='%s' h='%d'>", face, 
					msim_point_to_height(session,
						msim_purple_size_to_point(session, atoi(size))));
		} else if (face) {
			*begin = g_strdup_printf("<f f='%s'>", face);
		} else if (size) {
			*begin = g_strdup_printf("<f h='%d'>", 
					 msim_point_to_height(session,
						 msim_purple_size_to_point(session, atoi(size))));
		} else {
			*begin = g_strdup("<f>");
		}

		*end = g_strdup("</f>");

		/* TODO: color (bg uses <body>), emoticons */
	} else {
		gchar *err;

#ifdef MSIM_MARKUP_SHOW_UNKNOWN_TAGS
		*begin = g_strdup_printf("[%s]", root->name);
		*end = g_strdup_printf("[/%s]", root->name);
#else
		*begin = g_strdup("");
		*end = g_strdup("");
#endif

		err = g_strdup_printf("html_tag_to_msim_markup: unrecognized "
			"HTML tag %s was sent by the IM client; ignoring", 
			root->name ? root->name : "(NULL)");
		msim_unrecognized(NULL, NULL, err);
		g_free(err);
	}
	return ret;
}

/** Convert an xmlnode of msim markup or HTML to an HTML string or msim markup.
 *
 * @param f Function to convert tags.
 *
 * @return An HTML string. Caller frees.
 */
static gchar *
msim_convert_xmlnode(MsimSession *session, xmlnode *root, MSIM_XMLNODE_CONVERT f, int nodes_processed)
{
	xmlnode *node;
	gchar *begin, *inner, *end;
	GString *final;
	int descended = nodes_processed;

	if (!root || !root->name) {
		return g_strdup("");
	}

	purple_debug_info("msim", "msim_convert_xmlnode: got root=%s\n",
			root->name);

	begin = inner = end = NULL;

	final = g_string_new("");

	if (descended == 0) /* We've not formatted this yet.. :) */
		descended = f(session, root, &begin, &end); /* Get the value that our format function has already descended for us */
	
	g_string_append(final, begin);

	/* Loop over all child nodes. */
	for (node = root->child; node != NULL; node = node->next) {
		switch (node->type) {
			case XMLNODE_TYPE_ATTRIB:
				/* Attributes handled above. */
				break;

			case XMLNODE_TYPE_TAG:
				/* A tag or tag with attributes. Recursively descend. */
				inner = msim_convert_xmlnode(session, node, f, descended);
				g_return_val_if_fail(inner != NULL, NULL);
		
				purple_debug_info("msim", " ** node name=%s\n", 
						(node && node->name) ? node->name : "(NULL)");
				break;
		
			case XMLNODE_TYPE_DATA:
				/* Literal text. */
				inner = g_strndup(node->data, node->data_sz);
				purple_debug_info("msim", " ** node data=%s\n", 
						inner ? inner : "(NULL)");
				break;
		
			default:
				purple_debug_info("msim",
						"msim_convert_xmlnode: strange node\n");
		}

		if (inner) {
			g_string_append(final, inner);
			g_free(inner);
			inner = NULL;
		}
	}

	/* TODO: Note that msim counts each piece of text enclosed by <f> as
	 * a paragraph and will display each on its own line. You actually have
	 * to _nest_ <f> tags to intersperse different text in one paragraph!
	 * Comment out this line below to see. */
	g_string_append(final, end);

	g_free(begin);
	g_free(end);

	purple_debug_info("msim", "msim_markup_xmlnode_to_gtkhtml: RETURNING %s\n",
			(final && final->str) ? final->str : "(NULL)");

	return g_string_free(final, FALSE);
}

/** Convert XML to something based on MSIM_XMLNODE_CONVERT. */
static gchar *
msim_convert_xml(MsimSession *session, const gchar *raw, MSIM_XMLNODE_CONVERT f)
{
	xmlnode *root;
	gchar *str;
	gchar *enclosed_raw;

	g_return_val_if_fail(raw != NULL, NULL);

	/* Enclose text in one root tag, to try to make it valid XML for parsing. */
	enclosed_raw = g_strconcat("<root>", raw, "</root>", NULL);

	root = xmlnode_from_str(enclosed_raw, -1);

	if (!root) {
		purple_debug_info("msim", "msim_markup_to_html: couldn't parse "
				"%s as XML, returning raw: %s\n", enclosed_raw, raw);
		/* TODO: msim_unrecognized */
		g_free(enclosed_raw);
		return g_strdup(raw);
	}

	g_free(enclosed_raw);

	str = msim_convert_xmlnode(session, root, f, 0);
	g_return_val_if_fail(str != NULL, NULL);
	purple_debug_info("msim", "msim_markup_to_html: returning %s\n", str);

	xmlnode_free(root);

	return str;
}

/** Convert plaintext smileys to <i> markup tags.
 *
 * @param before Original text with ASCII smileys. Will be freed.
 * @return A new string with <i> tags, if applicable. Must be g_free()'d.
 */
static gchar *
msim_convert_smileys_to_markup(gchar *before)
{
	gchar *old, *new, *replacement;
	guint i;
	struct MSIM_EMOTICON *emote;

	old = before;
	new = NULL;

	for (i = 0; (emote = &msim_emoticons[i]) && emote->name != NULL; ++i) {
		gchar *name, *symbol;

		name = emote->name;
		symbol = emote->symbol;

		replacement = g_strdup_printf("<i n=\"%s\"/>", name);

		purple_debug_info("msim", "msim_convert_smileys_to_markup: %s->%s\n",
				symbol ? symbol : "(NULL)", 
				replacement ? replacement : "(NULL)");
		new = purple_strreplace(old, symbol, replacement);
		
		g_free(replacement);
		g_free(old);

		old = new;
	}

	return new;
}
	

/** High-level function to convert MySpaceIM markup to Purple (HTML) markup. 
 *
 * @return Purple markup string, must be g_free()'d. */
gchar *
msim_markup_to_html(MsimSession *session, const gchar *raw)
{
	return msim_convert_xml(session, raw, 
			(MSIM_XMLNODE_CONVERT)(msim_markup_tag_to_html));
}

/** High-level function to convert Purple (HTML) to MySpaceIM markup.
 *
 * TODO: consider using purple_markup_html_to_xhtml() to make valid XML.
 *
 * @return HTML markup string, must be g_free()'d. */
gchar *
html_to_msim_markup(MsimSession *session, const gchar *raw)
{
	gchar *markup;

	markup = msim_convert_xml(session, raw,
			(MSIM_XMLNODE_CONVERT)(html_tag_to_msim_markup));
	
	if (purple_account_get_bool(session->account, "emoticons", TRUE)) {
		/* Frees markup and allocates a new one. */
		markup = msim_convert_smileys_to_markup(markup);
	}

	return markup;
}


