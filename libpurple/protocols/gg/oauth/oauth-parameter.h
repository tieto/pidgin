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

#ifndef _GGP_OAUTH_PARAMETER_H
#define _GGP_OAUTH_PARAMETER_H

#include <internal.h>

typedef struct gg_oauth_parameter gg_oauth_parameter_t;

int gg_oauth_parameter_set(gg_oauth_parameter_t **list, const char *key, const char *value);
char *gg_oauth_parameter_join(gg_oauth_parameter_t *list, int header);
void gg_oauth_parameter_free(gg_oauth_parameter_t *list);

#endif /* _GGP_OAUTH_PARAMETER_H */
