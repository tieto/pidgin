/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#endif

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include "gaim.h"
#include "proxy.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

gchar *strip_html(const gchar *text)
{
	int i, j, k;
	int visible = 1;
	gchar *text2 = g_strdup(text);

	if(!text)
		return NULL;

	for (i = 0, j = 0; text2[i]; i++) {
		if (text2[i] == '<') {
			k = i + 1;
			if(g_ascii_isspace(text2[k])) {
				visible = 1;
			} else {
				while (text2[k]) {
					if (text2[k] == '<') {
						visible = 1;
						break;
					}
					if (text2[k] == '>') {
						visible = 0;
						break;
					}
					k++;
				}
			}
		} else if (text2[i] == '>' && !visible) {
			visible = 1;
			continue;
		}
		if (text2[i] == '&' && strncasecmp(text2+i,"&quot;",6) == 0) {
		    text2[j++] = '\"';
		    i = i+5;
		    continue;
		}
		if (visible) {
			text2[j++] = text2[i];
		}
	}
	text2[j] = '\0';
	return text2;
}

struct g_url *parse_url(char *url)
{
	struct g_url *test = g_new0(struct g_url, 1);
	char scan_info[255];
	char port[5];
	int f;

	if (strstr(url, "http://"))
		g_snprintf(scan_info, sizeof(scan_info),
			   "http://%%[A-Za-z0-9.]:%%[0-9]/%%[A-Za-z0-9.~_-/&%%?=+]");
	else
		g_snprintf(scan_info, sizeof(scan_info),
			   "%%[A-Za-z0-9.]:%%[0-9]/%%[A-Za-z0-9.~_-/&%%?=+^]");
	f = sscanf(url, scan_info, test->address, port, test->page);
	if (f == 1) {
		if (strstr(url, "http://"))
			g_snprintf(scan_info, sizeof(scan_info),
				   "http://%%[A-Za-z0-9.]/%%[A-Za-z0-9.~_-/&%%?=+^]");
		else
			g_snprintf(scan_info, sizeof(scan_info),
				   "%%[A-Za-z0-9.]/%%[A-Za-z0-9.~_-/&%%?=+^]");
		f = sscanf(url, scan_info, test->address, test->page);
		g_snprintf(port, sizeof(test->port), "80");
		port[2] = 0;
	}
	if (f == 1) {
		if (strstr(url, "http://"))
			g_snprintf(scan_info, sizeof(scan_info), "http://%%[A-Za-z0-9.]");
		else
			g_snprintf(scan_info, sizeof(scan_info), "%%[A-Za-z0-9.]");
		f = sscanf(url, scan_info, test->address);
		g_snprintf(test->page, sizeof(test->page), "%c", '\0');
	}

	sscanf(port, "%d", &test->port);
	return test;
}

struct grab_url_data {
	void (* callback)(gpointer, char *, unsigned long);
	gpointer data;
	struct g_url *website;
	char *url;
	gboolean full;

	int inpa;

	gboolean sentreq;
	gboolean newline;
	gboolean startsaving;
	char *webdata;
	unsigned long len;
	unsigned long data_len;
};

static gboolean
parse_redirect(const char *data, size_t data_len, gint sock,
			   struct grab_url_data *gunk)
{
	gchar *s;

	if ((s = g_strstr_len(data, data_len, "Location: ")) != NULL) {
		gchar *new_url, *end;
		int len;

		s += strlen("Location: ");
		end = strchr(s, '\r');

		/* Just in case :) */
		if (end == NULL)
			end = strchr(s, '\n');

		len = end - s;

		new_url = g_malloc(len + 1);
		strncpy(new_url, s, len);
		new_url[len] = '\0';

		/* Close the existing stuff. */
		gaim_input_remove(gunk->inpa);
		close(sock);

		/* Try again, with this new location. */
		grab_url(new_url, gunk->full, gunk->callback,
				 gunk->data);

		/* Free up. */
		g_free(new_url);
		g_free(gunk->webdata);
		g_free(gunk->website);
		g_free(gunk->url);
		g_free(gunk);

		return TRUE;
	}

	return FALSE;
}

static size_t
parse_content_len(const char *data, size_t data_len)
{
	size_t content_len = 0;

	sscanf(data, "Content-Length: %d", &content_len);

	return content_len;
}

static void grab_url_callback(gpointer dat, gint sock, GaimInputCondition cond)
{
	struct grab_url_data *gunk = dat;
	char data;

	if (sock == -1) {
		gunk->callback(gunk->data, NULL, 0);
		g_free(gunk->website);
		g_free(gunk->url);
		g_free(gunk);
		return;
	}

	if (!gunk->sentreq) {
		char buf[256];

		g_snprintf(buf, sizeof(buf), "GET %s%s HTTP/1.0\r\n\r\n", gunk->full ? "" : "/",
			   gunk->full ? gunk->url : gunk->website->page);
		debug_printf("Request: %s\n", buf);

		write(sock, buf, strlen(buf));
		fcntl(sock, F_SETFL, O_NONBLOCK);
		gunk->sentreq = TRUE;
		gunk->inpa = gaim_input_add(sock, GAIM_INPUT_READ, grab_url_callback, dat);
		gunk->data_len = 4096;
		gunk->webdata = g_malloc(gunk->data_len);
		return;
	}

	if (read(sock, &data, 1) > 0 || errno == EWOULDBLOCK) {
		if (errno == EWOULDBLOCK) {
			errno = 0;
			return;
		}

		gunk->len++;

		if (gunk->len == gunk->data_len + 1) {
			gunk->data_len += (gunk->data_len) / 2;

			gunk->webdata = g_realloc(gunk->webdata, gunk->data_len);
		}

		gunk->webdata[gunk->len - 1] = data;

		if (!gunk->startsaving) {
			if (data == '\r')
				return;
			if (data == '\n') {
				if (gunk->newline) {
					size_t content_len;
					gunk->startsaving = TRUE;

					/* See if we can find a redirect. */
					if (parse_redirect(gunk->webdata, gunk->len, sock, gunk))
						return;

					/* No redirect. See if we can find a content length. */
					content_len = parse_content_len(gunk->webdata, gunk->len);

					if (content_len == 0) {
						/* We'll stick with an initial 8192 */
						content_len = 8192;
					}

					/* Out with the old... */
					gunk->len = 0;
					g_free(gunk->webdata);
					gunk->webdata = NULL;

					/* In with the new. */
					gunk->data_len = content_len;
					gunk->webdata = g_malloc(gunk->data_len);
				}
				else
					gunk->newline = TRUE;
				return;
			}
			gunk->newline = FALSE;
		}
	} else if (errno != ETIMEDOUT) {
		gunk->webdata = g_realloc(gunk->webdata, gunk->len + 1);
		gunk->webdata[gunk->len] = 0;

		debug_printf(_("Received: '%s'\n"), gunk->webdata);

		gaim_input_remove(gunk->inpa);
		close(sock);
		gunk->callback(gunk->data, gunk->webdata, gunk->len);
		if (gunk->webdata)
			g_free(gunk->webdata);
		g_free(gunk->website);
		g_free(gunk->url);
		g_free(gunk);
	} else {
		gaim_input_remove(gunk->inpa);
		close(sock);
		gunk->callback(gunk->data, NULL, 0);
		if (gunk->webdata)
			g_free(gunk->webdata);
		g_free(gunk->website);
		g_free(gunk->url);
		g_free(gunk);
	}
}

void grab_url(char *url, gboolean full, void callback(gpointer, char *, unsigned long), gpointer data)
{
	int sock;
	struct grab_url_data *gunk = g_new0(struct grab_url_data, 1);

	gunk->callback = callback;
	gunk->data = data;
	gunk->url = g_strdup(url);
	gunk->website = parse_url(url);
	gunk->full = full;

	if ((sock = proxy_connect(NULL, gunk->website->address, gunk->website->port,
				  grab_url_callback, gunk)) < 0) {
		g_free(gunk->website);
		g_free(gunk->url);
		g_free(gunk);
		callback(data, g_strdup(_("g003: Error opening connection.\n")), 0);
	}
}

struct gaim_parse_tag {
	char *src_tag;
	char *dest_tag;
};

#define ALLOW_TAG_ALT(x, y) if(!g_ascii_strncasecmp(c, "<" x " ", strlen("<" x " "))) { \
						const char *o = c + 1; \
						const char *p = NULL, *q = NULL, *r = NULL; \
						while(*o) { \
							if(!q && (*o == '\"' || *o == '\'') ) { \
								q = o; \
							} else if(q) { \
								if(*o == *q) { \
									q = NULL; \
								} else if(*c == '\\') { \
									o++; \
								} \
							} else if(*o == '<') { \
								r = o; \
							} else if(*o == '>') { \
								p = o; \
								break; \
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
							xhtml = g_string_append_len(xhtml, c, (p - c) + 1); \
							c = p + 1; \
						} else { \
							xhtml = g_string_append(xhtml, "&lt;"); \
							plain = g_string_append_c(plain, '<'); \
						} \
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

void html_to_xhtml(const char *html, char **xhtml_out, char **plain_out) {
	GString *xhtml = g_string_new("");
	GString *plain = g_string_new("");
	GList *tags = NULL, *tag;
	const char *c = html;
	while(*c) {
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
					/* we tried to close a tag we never opened! escape it
					 * and move on */
					xhtml = g_string_append(xhtml, "&lt;");
					plain = g_string_append_c(plain, '<');
					c++;
				}
			} else { /* opening tag */
				ALLOW_TAG("a");
				ALLOW_TAG_ALT("b", "strong");
				ALLOW_TAG("blockquote");
				ALLOW_TAG_ALT("bold", "strong");
				ALLOW_TAG("cite");
				ALLOW_TAG("div");
				ALLOW_TAG("em");
				ALLOW_TAG("h1");
				ALLOW_TAG("h2");
				ALLOW_TAG("h3");
				ALLOW_TAG("h4");
				ALLOW_TAG("h5");
				ALLOW_TAG("h6");
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
						if(!g_ascii_strncasecmp(p, "color=", strlen("color="))) {
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
							g_string_append_printf(style, "font-family: %s; ", face->str);
							g_string_free(face, TRUE);
							p = q;
						} else if(!g_ascii_strncasecmp(p, "size=", strlen("size="))) {
							const char *q = p + strlen("size=");
							int sz;
							const char *size = "medium";
							if(*q == '\'' || *q == '\"')
								q++;
							sz = atoi(q);
							if(sz < 3)
								size = "smaller";
							else if(sz > 3)
								size = "larger";
							g_string_append_printf(style, "font-size: %s; ", size);
							p = q;
						}
						p++;
					}
					c = strchr(c, '>') + 1;
					pt = g_new0(struct gaim_parse_tag, 1);
					pt->src_tag = "font";
					pt->dest_tag = "span";
					tags = g_list_prepend(tags, pt);
					xhtml = g_string_append(xhtml, "<span");
					if(style->len)
						g_string_append_printf(xhtml, " style='%s'", style->str);
					xhtml = g_string_append_c(xhtml, '>');
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
							g_string_append_printf(xhtml, "<span style='background: %s;'>", color->str);
							g_string_free(color, TRUE);
							c = strchr(c, '>') + 1;
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
		} else {
			xhtml = g_string_append_c(xhtml, *c);
			plain = g_string_append_c(plain, *c);
			c++;
		}
	}
	tag = tags;
	while(tag) {
		g_string_append_printf(xhtml, "</%s>", (char *)tag->data);
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
