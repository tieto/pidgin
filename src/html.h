/**
 * @file html.h HTML Utility API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _GAIM_HTML_H_
#define _GAIM_HTML_H_

struct g_url {
	char address[255];
	int port;
	char page[255];
};

void grab_url(char *url, gboolean full,
			  void (*callback)(gpointer, char *, unsigned long),
			  gpointer data);

gchar *strip_html(const gchar *text);
void html_to_xhtml(const char *html, char **xhtml_out, char **plain_out);
struct g_url *parse_url(char *url);

#endif /* _GAIM_HTML_H_ */
