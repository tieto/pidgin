/**
 * The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
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

// START OF FILE
/*****************************************************************************/
#ifndef _QQ_CHAR_CONV_H_
#define _QQ_CHAR_CONV_H_

#include <glib.h>

#define QQ_CHARSET_DEFAULT      "GBK"

gint convert_as_pascal_string(guint8 * data, gchar ** ret, const gchar * from_charset);

gchar *qq_smiley_to_gaim(gchar * text);

gchar *gaim_smiley_to_qq(gchar * text);

gchar *utf8_to_qq(const gchar * str, const gchar * to_charset);
gchar *qq_to_utf8(const gchar * str, const gchar * from_charset);
gchar *qq_encode_to_gaim(guint8 * font_attr_data, gint len, const gchar * msg);

gchar *qq_im_filter_html(const gchar * text);

#endif
/*****************************************************************************/
// END OF FILE
