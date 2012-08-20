/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#ifndef _GGP_MULTILOGON_H
#define _GGP_MULTILOGON_H

#include <internal.h>
#include <libgadu.h>

typedef struct _ggp_multilogon_session_data ggp_multilogon_session_data;

void ggp_multilogon_setup(PurpleConnection *gc);
void ggp_multilogon_cleanup(PurpleConnection *gc);

void ggp_multilogon_msg(PurpleConnection *gc, struct gg_event_msg *msg);
void ggp_multilogon_info(PurpleConnection *gc,
	struct gg_event_multilogon_info *msg);

int ggp_multilogon_get_session_count(PurpleConnection *gc);

#endif /* _GGP_MULTILOGON_H */
