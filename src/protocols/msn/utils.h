/**
 * @file utils.h Utility functions
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
#ifndef _MSN_UTILS_H_
#define _MSN_UTILS_H_

/**
 * Decodes a URL into a plain string.
 *
 * This will change hex codes and such to their ascii equivalents.
 *
 * @param str The string to translate.
 *
 * @return The resulting string.
 */
char *msn_url_decode(const char *str);

/**
 * Encodes a URL into an escaped string.
 *
 * This will change non-alphanumeric characters to hex codes.
 *
 * @param str The string to translate.
 *
 * @return The resulting string.
 */
char *msn_url_encode(const char *str);

/**
 * Parses the MSN message formatting into a format compatible with Gaim.
 *
 * @param mime The mime header with the formatting.
 *
 * @return The new message.
 */
char *msn_parse_format(const char *mime);

#endif /* _MSN_UTILS_H_ */
