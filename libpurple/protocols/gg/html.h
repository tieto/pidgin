/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here. Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Component written by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * This file is dual-licensed under the GPL2+ and the X11 (MIT) licences.
 * As a recipient of this file you may choose, which license to receive the
 * code under. As a contributor, you have to ensure the new code is
 * compatible with both.
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
#ifndef _GGP_HTML_H
#define _GGP_HTML_H

#include <internal.h>

typedef enum
{
	GGP_HTML_TAG_UNKNOWN,
	GGP_HTML_TAG_EOM,
	GGP_HTML_TAG_A,
	GGP_HTML_TAG_B,
	GGP_HTML_TAG_I,
	GGP_HTML_TAG_U,
	GGP_HTML_TAG_S,
	GGP_HTML_TAG_IMG,
	GGP_HTML_TAG_FONT,
	GGP_HTML_TAG_SPAN,
	GGP_HTML_TAG_DIV,
	GGP_HTML_TAG_BR,
	GGP_HTML_TAG_HR,
} ggp_html_tag;

void ggp_html_setup(void);
void ggp_html_cleanup(void);

GHashTable * ggp_html_tag_attribs(const gchar *attribs_str);
GHashTable * ggp_html_css_attribs(const gchar *attribs_str);
int ggp_html_decode_color(const gchar *str);
ggp_html_tag ggp_html_parse_tag(const gchar *tag_str);


#endif /* _GGP_HTML_H */
