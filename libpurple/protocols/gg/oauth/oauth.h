/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Code adapted from libgadu (C) 2008 Wojtek Kaniewski <wojtekka@irc.pl>
 * (http://toxygen.net/libgadu/) during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
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

#ifndef _GGP_OAUTH_H
#define _GGP_OAUTH_H

#include <internal.h>
#include <libgadu.h>

char *gg_oauth_generate_header(const char *method, const char *url, const const char *consumer_key, const char *consumer_secret, const char *token, const char *token_secret);

#endif /* _GGP_OAUTH_H */
