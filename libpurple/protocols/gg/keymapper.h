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
#ifndef _GGP_KEYMAPPER_H
#define _GGP_KEYMAPPER_H

typedef struct _ggp_keymapper ggp_keymapper;

#include <purple.h>

ggp_keymapper *
ggp_keymapper_new(void);

void
ggp_keymapper_free(ggp_keymapper *km);

gpointer
ggp_keymapper_to_key(ggp_keymapper *km, guint64 val);

/* The key have to be valid. */
guint64
ggp_keymapper_from_key(ggp_keymapper *km, gpointer key);

#endif /* _GGP_KEYMAPPER_H */
