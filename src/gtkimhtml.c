/*
 * GtkIMHtml
 *
 * Copyright (C) 2000, Eric Warmenhoven <warmenhoven@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "gtkimhtml.h"
#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#include <locale.h>
#endif


/* POINT_SIZE converts from AIM font sizes to point sizes.  It probably should be redone in such a
 * way that it base the sizes off the default font size rather than using arbitrary font sizes. */
#define MAX_FONT_SIZE 7
#define POINT_SIZE(x) (_point_sizes [MIN ((x), MAX_FONT_SIZE) - 1])
static gint _point_sizes [] = { 8, 10, 12, 14, 20, 30, 40 };

/* The four elements present in a <FONT> tag contained in a struct */
typedef struct _FontDetail   FontDetail;
struct _FontDetail {
	gushort size;
	gchar *face;
	gchar *fore;
	gchar *back;
};

/* GtkIMHtml has one signal--URL_CLICKED */
enum {
	URL_CLICKED,
	LAST_SIGNAL
};
static guint signals [LAST_SIGNAL] = { 0 };


/* Boring GTK stuff */
static void gtk_imhtml_class_init (GtkIMHtmlClass *class)
{
	GtkObjectClass *object_class;
	object_class = (GtkObjectClass*) class;
	signals[URL_CLICKED] = gtk_signal_new("url_clicked",
					      GTK_RUN_FIRST,
					      GTK_CLASS_TYPE(object_class),
					      GTK_SIGNAL_OFFSET(GtkIMHtmlClass, url_clicked),
					      gtk_marshal_NONE__POINTER,
					      GTK_TYPE_NONE, 1,
					      GTK_TYPE_POINTER);
}

static void gtk_imhtml_init (GtkIMHtml *imhtml)
{
	GtkTextIter iter;
	imhtml->text_buffer = gtk_text_buffer_new(NULL);
	gtk_text_buffer_get_end_iter (imhtml->text_buffer, &iter);
	imhtml->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, FALSE);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(imhtml), imhtml->text_buffer);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(imhtml), GTK_WRAP_WORD);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(imhtml), FALSE);
	gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(imhtml), 5);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(imhtml), FALSE);
	/*gtk_text_view_set_justification(GTK_TEXT_VIEW(imhtml), GTK_JUSTIFY_FILL);*/
	
	/* These tags will be used often and can be reused--we create them on init and then apply them by name
	 * other tags (color, size, face, etc.) will have to be created and applied dynamically */ 
	gtk_text_buffer_create_tag(imhtml->text_buffer, "BOLD", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "ITALICS", "style", PANGO_STYLE_ITALIC, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "UNDERLINE", "underline", PANGO_UNDERLINE_SINGLE, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "STRIKE", "strikethrough", TRUE, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "SUB", "rise", -5000, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "SUP", "rise", 5000, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "PRE", "family", "Monospace", NULL);
	
	/* When hovering over a link, we show the hand cursor--elsewhere we show the plain ol' pointer cursor */
	imhtml->hand_cursor = gdk_cursor_new (GDK_HAND2);
	imhtml->arrow_cursor = gdk_cursor_new (GDK_LEFT_PTR);

}

GtkWidget *gtk_imhtml_new(void *a, void *b)
{
	return GTK_WIDGET(gtk_type_new(gtk_imhtml_get_type()));
}

GtkType gtk_imhtml_get_type()
{
	static guint imhtml_type = 0;

	if (!imhtml_type) {
		GtkTypeInfo imhtml_info = {
			"GtkIMHtml",
			sizeof (GtkIMHtml),
			sizeof (GtkIMHtmlClass),
			(GtkClassInitFunc) gtk_imhtml_class_init,
			(GtkObjectInitFunc) gtk_imhtml_init,
			NULL,
			NULL
		};
		
		imhtml_type = gtk_type_unique (gtk_text_view_get_type (), &imhtml_info);
	}

	return imhtml_type;
}

/* The call back for an event on a link tag. */
void tag_event(GtkTextTag *tag, GObject *arg1, GdkEvent *event, GtkTextIter *arg2, char *url) {
	if (event->type == GDK_BUTTON_RELEASE) {
		/* A link was clicked--we emit the "url_clicked" signal with the URL as the argument */
		//	if ((GdkEventButton)(event)->button == 1) 
			gtk_signal_emit (G_OBJECT(arg1), signals[URL_CLICKED], url);
	} else if (event->type == GDK_ENTER_NOTIFY) {
		/* make a hand cursor and a tooltip timeout -- if GTK worked as it should */
	} else if (event->type == GDK_LEAVE_NOTIFY) {
		/* clear timeout and make an arrow cursor again --if GTK worked as it should */
	}
}

#define VALID_TAG(x)	if (!g_strncasecmp (string, x ">", strlen (x ">"))) {	\
				*tag = g_strndup (string, strlen (x));		\
				*len = strlen (x) + 1;				\
				return TRUE;					\
			}							\
			(*type)++

#define VALID_OPT_TAG(x)	if (!g_strncasecmp (string, x " ", strlen (x " "))) {	\
					const gchar *c = string + strlen (x " ");	\
					gchar e = '"';					\
					gboolean quote = FALSE;				\
					while (*c) {					\
						if (*c == '"' || *c == '\'') {		\
							if (quote && (*c == e))		\
								quote = !quote;		\
							else if (!quote) {		\
								quote = !quote;		\
								e = *c;			\
							}				\
						} else if (!quote && (*c == '>'))	\
							break;				\
						c++;					\
					}						\
					if (*c) {					\
						*tag = g_strndup (string, c - string);	\
						*len = c - string + 1;			\
						return TRUE;				\
					}						\
				}							\
				(*type)++


static gboolean
gtk_imhtml_is_amp_escape (const gchar *string,
			  gchar       *replace,
			  gint        *length)
{
	g_return_val_if_fail (string != NULL, FALSE);
	g_return_val_if_fail (replace != NULL, FALSE);
	g_return_val_if_fail (length != NULL, FALSE);

	if (!g_strncasecmp (string, "&amp;", 5)) {
		*replace = '&';
		*length = 5;
	} else if (!g_strncasecmp (string, "&lt;", 4)) {
		*replace = '<';
		*length = 4;
	} else if (!g_strncasecmp (string, "&gt;", 4)) {
		*replace = '>';
		*length = 4;
	} else if (!g_strncasecmp (string, "&nbsp;", 6)) {
		*replace = ' ';
		*length = 6;
	} else if (!g_strncasecmp (string, "&copy;", 6)) {
		*replace = '©'; /* was: 'Â©' */
		*length = 6;
	} else if (!g_strncasecmp (string, "&quot;", 6)) {
		*replace = '\"';
		*length = 6;
	} else if (!g_strncasecmp (string, "&reg;", 5)) {
		*replace = '®'; /* was: 'Â®' */
		*length = 5;
	} else if (*(string + 1) == '#') {
		guint pound = 0;
		if ((sscanf (string, "&#%u;", &pound) == 1) && pound != 0) {
			if (*(string + 3 + (gint)log10 (pound)) != ';')
				return FALSE;
			*replace = (gchar)pound;
			*length = 2;
			while (isdigit ((gint) string [*length])) (*length)++;
			if (string [*length] == ';') (*length)++;
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}

	return TRUE;
}

static gboolean
gtk_imhtml_is_tag (const gchar *string,
		   gchar      **tag,
		   gint        *len,
		   gint        *type)
{
	*type = 1;

	if (!strchr (string, '>'))
		return FALSE;

	VALID_TAG ("B");
	VALID_TAG ("BOLD");
	VALID_TAG ("/B");
	VALID_TAG ("/BOLD");
	VALID_TAG ("I");
	VALID_TAG ("ITALIC");
	VALID_TAG ("/I");
	VALID_TAG ("/ITALIC");
	VALID_TAG ("U");
	VALID_TAG ("UNDERLINE");
	VALID_TAG ("/U");
	VALID_TAG ("/UNDERLINE");
	VALID_TAG ("S");
	VALID_TAG ("STRIKE");
	VALID_TAG ("/S");
	VALID_TAG ("/STRIKE");
	VALID_TAG ("SUB");
	VALID_TAG ("/SUB");
	VALID_TAG ("SUP");
	VALID_TAG ("/SUP");
	VALID_TAG ("PRE");
	VALID_TAG ("/PRE");
	VALID_TAG ("TITLE");
	VALID_TAG ("/TITLE");
	VALID_TAG ("BR");
	VALID_TAG ("HR");
	VALID_TAG ("/FONT");
	VALID_TAG ("/A");
	VALID_TAG ("P");
	VALID_TAG ("/P");
	VALID_TAG ("H3");
	VALID_TAG ("/H3");
	VALID_TAG ("HTML");
	VALID_TAG ("/HTML");
	VALID_TAG ("BODY");
	VALID_TAG ("/BODY");
	VALID_TAG ("FONT");
	VALID_TAG ("HEAD");
	VALID_TAG ("/HEAD");
	VALID_TAG ("BINARY");
	VALID_TAG ("/BINARY");
	
	VALID_OPT_TAG ("HR");
	VALID_OPT_TAG ("FONT");
	VALID_OPT_TAG ("BODY");
	VALID_OPT_TAG ("A");
	VALID_OPT_TAG ("IMG");
	VALID_OPT_TAG ("P");
	VALID_OPT_TAG ("H3");

	if (!g_strncasecmp(string, "!--", strlen ("!--"))) {
		gchar *e = strstr (string + strlen("!--"), "-->");
		if (e) {
			*len = e - string + strlen ("-->");
			*tag = g_strndup (string + strlen ("!--"), *len - strlen ("!---->"));
			return TRUE;
		}
	}

	return FALSE;
}

static gchar*
gtk_imhtml_get_html_opt (gchar       *tag,
			 const gchar *opt)
{
	gchar *t = tag;
	gchar *e, *a;

	while (g_strncasecmp (t, opt, strlen (opt))) {
		gboolean quote = FALSE;
		if (*t == '\0') break;
		while (*t && !((*t == ' ') && !quote)) {
			if (*t == '\"')
				quote = ! quote;
			t++;
		}
		while (*t && (*t == ' ')) t++;
	}

	if (!g_strncasecmp (t, opt, strlen (opt))) {
		t += strlen (opt);
	} else {
		return NULL;
	}

	if ((*t == '\"') || (*t == '\'')) {
		e = a = ++t;
		while (*e && (*e != *(t - 1))) e++;
		if  (*e == '\0') {
			return NULL;
		} else 
			return g_strndup (a, e - a);
	} else {
		e = a = t;
		while (*e && !isspace ((gint) *e)) e++;
		return g_strndup (a, e - a);
	}
}



#define NEW_TEXT_BIT 0
#define NEW_HR_BIT 1
#define NEW_BIT(x)	ws [wpos] = '\0'; \
                        mark2 = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE); \
                        gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, -1); \
                      	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &iter); \
                        gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &siter, mark2); \
                        gtk_text_buffer_delete_mark(imhtml->text_buffer, mark2); \
                        if (bold) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "BOLD", &siter, &iter); \
		        if (italics) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "ITALICS", &siter, &iter); \
                        if (underline) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "UNDERLINE", &siter, &iter); \
                        if (strike) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "STRIKE", &siter, &iter); \
                        if (sub) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "SUB", &siter, &iter); \
                        if (sup) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "SUP", &siter, &iter); \
                        if (pre) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "PRE", &siter, &iter); \
                        if (bg) { \
                                texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "background", bg, NULL); \
                                gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                        } \
                        if (fonts) { \
                                 FontDetail *fd = fonts->data; \
                                 if (fd->fore) { \
	                                    texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "foreground", fd->fore, NULL); \
                                            gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                                 } \
                                 if (fd->back) { \
                                            texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "background", fd->back, NULL); \
                                            gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                                 } \
                                 if (fd->face) { \
                                            texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "font", fd->face, NULL); \
                                            gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                                 } \
                                 if (fd->size) { \
                                            texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "size-points", (double)POINT_SIZE(fd->size), NULL); \
                                            gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                                 } \
                        } \
                        if (url) { \
                                 texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "foreground", "blue", "underline", PANGO_UNDERLINE_SINGLE, NULL); \
                                 g_signal_connect(G_OBJECT(texttag), "event", G_CALLBACK(tag_event), strdup(url)); \
                                 gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                        } \
                        wpos = 0; \
                        ws[0] = 0; \
                        gtk_text_buffer_get_end_iter(imhtml->text_buffer, &iter); \


/*                        if (x == NEW_HR_BIT) { \
	                         sep = gtk_hseparator_new(); \
                                 gtk_widget_size_request(GTK_WIDGET(imhtml), &req); \
                                 gtk_widget_set_size_request(sep, 20, -1); \
	                         anchor = gtk_text_buffer_create_child_anchor(imhtml->text_buffer, &iter); \
                                 gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(imhtml), sep, anchor); \
                                 gtk_widget_show(sep); \
			  } */

GString* gtk_imhtml_append_text (GtkIMHtml        *imhtml,
				 const gchar      *text,
				 gint              len,
				 GtkIMHtmlOptions  options)
{
	gint pos = 0;
	GString *str = NULL;
	GtkTextIter iter, siter;
	GtkTextMark *mark, *mark2;
	GtkTextChildAnchor *anchor;
	GtkTextTag *texttag;
	GtkWidget *sep;
	GtkRequisition req;
	gchar *ws;
	gchar *tag;
	gchar *url = NULL;
	gchar *bg = NULL;
	gint tlen, wpos=0;
	gint type;
	const gchar *c;
	gchar amp;

	guint	bold = 0,
		italics = 0,
		underline = 0,
		strike = 0,
		sub = 0,
		sup = 0,
		title = 0,
		pre = 0;	

	GSList *fonts = NULL;

	g_return_val_if_fail (imhtml != NULL, NULL);
	g_return_val_if_fail (GTK_IS_IMHTML (imhtml), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (len != 0, NULL);
	
	c = text;
	if (len == -1)
		len = strlen(text);
	ws = g_malloc(len + 1);
	ws[0] = 0;

	if (options & GTK_IMHTML_RETURN_LOG)
		str = g_string_new("");

	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &iter);
	mark = gtk_text_buffer_create_mark (imhtml->text_buffer, NULL, &iter, /* right grav */ FALSE);
	while (pos < len) {
		if (*c == '<' && gtk_imhtml_is_tag (c + 1, &tag, &tlen, &type)) {
			c++;
			pos++;
			switch (type) 
				{
				case 1:		/* B */
				case 2:		/* BOLD */
					NEW_BIT (NEW_TEXT_BIT);
					bold++;
					break;
				case 3:		/* /B */
				case 4:		/* /BOLD */
					NEW_BIT (NEW_TEXT_BIT);
					if (bold)
						bold--;
					break;
				case 5:		/* I */
				case 6:		/* ITALIC */
					NEW_BIT (NEW_TEXT_BIT);
					italics++;
					break;
				case 7:		/* /I */
				case 8:		/* /ITALIC */
					NEW_BIT (NEW_TEXT_BIT);
					if (italics)
						italics--;
					break;
				case 9:		/* U */
				case 10:	/* UNDERLINE */
					NEW_BIT (NEW_TEXT_BIT);
					underline++;
					break;
				case 11:	/* /U */
				case 12:	/* /UNDERLINE */
					NEW_BIT (NEW_TEXT_BIT);
					if (underline)
						underline--;
					break;
				case 13:	/* S */
				case 14:	/* STRIKE */
					NEW_BIT (NEW_TEXT_BIT);
					strike++;
					break;
				case 15:	/* /S */
				case 16:	/* /STRIKE */
					NEW_BIT (NEW_TEXT_BIT);
					if (strike)
						strike--;
					break;
				case 17:	/* SUB */
					NEW_BIT (NEW_TEXT_BIT);
					sub++;
					break;
				case 18:	/* /SUB */
					NEW_BIT (NEW_TEXT_BIT);
					if (sub)
						sub--;
					break;
				case 19:	/* SUP */
					NEW_BIT (NEW_TEXT_BIT);
					sup++;
				break;
				case 20:	/* /SUP */
					NEW_BIT (NEW_TEXT_BIT);
					if (sup)
						sup--;
					break;
				case 21:	/* PRE */
					NEW_BIT (NEW_TEXT_BIT);
					pre++;
					break;
				case 22:	/* /PRE */
					NEW_BIT (NEW_TEXT_BIT);
					if (pre)
						pre--;
					break;
				case 23:	/* TITLE */
					NEW_BIT (NEW_TEXT_BIT);
					title++;
					break;
				case 24:	/* /TITLE */
					if (title) {
						if (options & GTK_IMHTML_NO_TITLE) {
							wpos = 0;
							ws [wpos] = '\0';
						}
						title--;
					}
					break;
				case 25:	/* BR */
					ws[wpos] = '\n';
					wpos++;
					NEW_BIT (NEW_TEXT_BIT);
					break;	
				case 26:        /* HR */
				case 42:        /* HR (opt) */
					ws[wpos++] = '\n';
					NEW_BIT(NEW_HR_BIT);
					break;
				case 27:	/* /FONT */
					if (fonts) {
						FontDetail *font = fonts->data;
						NEW_BIT (NEW_TEXT_BIT);
						fonts = g_slist_remove (fonts, font);
						if (font->face)
							g_free (font->face);
						if (font->fore)
							g_free (font->fore);
						if (font->back)
							g_free (font->back);
						g_free (font);
					}
					break;
				case 28:        /* /A    */
					if (url) {
						NEW_BIT(NEW_TEXT_BIT);
						g_free(url);
						url = NULL;
						break;
					}
				case 29:	/* P */
				case 30:	/* /P */
				case 31:	/* H3 */
				case 32:	/* /H3 */
				case 33:	/* HTML */
				case 34:	/* /HTML */
				case 35:	/* BODY */
				case 36:	/* /BODY */
				case 37:	/* FONT */
				case 38:	/* HEAD */
				case 39:	/* /HEAD */
					break; 
				case 40:        /* BINARY */
				case 41:        /* /BINARY */
					break;
				case 43:	/* FONT (opt) */
					{
						gchar *color, *back, *face, *size;
						FontDetail *font, *oldfont = NULL;
						color = gtk_imhtml_get_html_opt (tag, "COLOR=");
						back = gtk_imhtml_get_html_opt (tag, "BACK=");
						face = gtk_imhtml_get_html_opt (tag, "FACE=");
						size = gtk_imhtml_get_html_opt (tag, "SIZE=");
						
						if (!(color || back || face || size))
							break;
						
						NEW_BIT (NEW_TEXT_BIT);
						
						font = g_new0 (FontDetail, 1);
						if (fonts)
							oldfont = fonts->data;

						if (color && !(options & GTK_IMHTML_NO_COLOURS))
							font->fore = color;
						else if (oldfont && oldfont->fore)
							font->fore = g_strdup(oldfont->fore);

						if (back && !(options & GTK_IMHTML_NO_COLOURS))
							font->back = back;
						else if (oldfont && oldfont->back)
							font->back = g_strdup(oldfont->back);
						
						if (face && !(options & GTK_IMHTML_NO_FONTS))
							font->face = face;
						else if (oldfont && oldfont->face)
							font->face = g_strdup(oldfont->face);
						
						if (size && !(options & GTK_IMHTML_NO_SIZES)) {
							if (*size == '+') {
								sscanf (size + 1, "%hd", &font->size);
								font->size += 3;
							} else if (*size == '-') {
								sscanf (size + 1, "%hd", &font->size);
								font->size = MAX (0, 3 - font->size);
							} else if (isdigit (*size)) {
								sscanf (size, "%hd", &font->size);
							}
						} else if (oldfont)
							font->size = oldfont->size;
						g_free(size);
						fonts = g_slist_prepend (fonts, font);
					}
					break;
				case 44:	/* BODY (opt) */
					if (!(options & GTK_IMHTML_NO_COLOURS)) {
						char *bgcolor = gtk_imhtml_get_html_opt (tag, "BGCOLOR=");
						if (bgcolor) {
							NEW_BIT(NEW_TEXT_BIT);
							if (bg)
								g_free(bg);
							bg = bgcolor;
						}
					}
					break;
				case 45:	/* A (opt) */
					{
						gchar *href = gtk_imhtml_get_html_opt (tag, "HREF=");
						if (href) {
							NEW_BIT (NEW_TEXT_BIT);
							if (url)
								g_free (url);
							url = href;
						}
					}
					break;
				case 47:	/* P (opt) */
				case 48:	/* H3 (opt) */
					break;
				case 49:	/* comment */
					NEW_BIT (NEW_TEXT_BIT);
					wpos = g_snprintf (ws, len, "%s", tag);
					NEW_BIT (NEW_COMMENT_BIT);
					break;
				default:
					break;	
				}
			c += tlen;
			pos += tlen;
		} else if (*c == '&' && gtk_imhtml_is_amp_escape (c, &amp, &tlen)) {
			ws [wpos++] = amp;
			c += tlen;
			pos += tlen;
		} else if (*c == '\n') {
			if (!(options & GTK_IMHTML_NO_NEWLINE)) {
				ws[wpos] = '\n';
				wpos++;
				NEW_BIT (NEW_TEXT_BIT);
			}
			c++;
			pos++;
		} else if (*c) {
			ws [wpos++] = *c++;
			pos++;
		} else {
			break;
		}
	}
	
	NEW_BIT(NEW_TEXT_BIT);
	if (url) {
		g_free (url);
		if (str)
			str = g_string_append (str, "</A>");
	}
	
	if (str) {
		while (bold) {
			str = g_string_append (str, "</B>");
			bold--;
		}
		while (italics) {
			str = g_string_append (str, "</I>");
			italics--;
		}
		while (underline) {
			str = g_string_append (str, "</U>");
			underline--;
		}
		while (strike) {
			str = g_string_append (str, "</S>");
			strike--;
		}
		while (sub) {
			str = g_string_append (str, "</SUB>");
			sub--;
		}
		while (sup) {
			str = g_string_append (str, "</SUP>");
			sup--;
		}
		while (title) {
			str = g_string_append (str, "</TITLE>");
			title--;
		}
		while (pre) {
			str = g_string_append (str, "</PRE>");
			pre--;
		}
	}
	g_free(ws);
	gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (imhtml), mark,
				      0, TRUE, 0.0, 1.0);
	gtk_text_buffer_delete_mark (imhtml->text_buffer, mark);
	return str;
}

void       gtk_imhtml_set_adjustments  (GtkIMHtml        *imhtml,
					GtkAdjustment    *hadj,
	GtkAdjustment    *vadj){}

void       gtk_imhtml_set_defaults     (GtkIMHtml            *imhtml,
					PangoFontDescription *font,
					GdkColor             *fg_color,
	GdkColor             *bg_color){}

void       gtk_imhtml_set_img_handler  (GtkIMHtml        *imhtml,
	GtkIMHtmlImage    handler){}

void       gtk_imhtml_associate_smiley (GtkIMHtml        *imhtml,
					gchar            *text,
	gchar           **xpm){}
void 	   gtk_imhtml_init_smileys     (GtkIMHtml *imhtml){}
void       gtk_imhtml_remove_smileys   (GtkIMHtml        *imhtml){}
void       gtk_imhtml_reset_smileys   (GtkIMHtml        *imhtml){}
void       gtk_imhtml_show_smileys     (GtkIMHtml        *imhtml,
	gboolean          show){}

void       gtk_imhtml_show_comments    (GtkIMHtml        *imhtml,
	gboolean          show){}

void
gtk_imhtml_clear (GtkIMHtml *imhtml)
{
	GtkTextIter start, end;
	
	gtk_text_buffer_get_start_iter(imhtml->text_buffer, &start);
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &end);
	gtk_text_buffer_delete(imhtml->text_buffer, &start, &end);
}

void       gtk_imhtml_page_up          (GtkIMHtml        *imhtml){}
void       gtk_imhtml_page_down        (GtkIMHtml        *imhtml){}
