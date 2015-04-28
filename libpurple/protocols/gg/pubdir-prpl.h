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

#ifndef _GGP_PUBDIR_PROTOCOL_H
#define _GGP_PUBDIR_PROTOCOL_H

#include <internal.h>
#include <libgadu.h>

typedef enum
{
	GGP_PUBDIR_GENDER_UNSPECIFIED,
	GGP_PUBDIR_GENDER_FEMALE,
	GGP_PUBDIR_GENDER_MALE,
} ggp_pubdir_gender;

typedef struct
{
	uin_t uin;
	gchar *label;
	gchar *nickname;
	gchar *first_name;
	gchar *last_name;
	ggp_pubdir_gender gender;
	gchar *city;
	unsigned int province;
	time_t birth;
	unsigned int age;
} ggp_pubdir_record;

typedef struct _ggp_pubdir_search_form ggp_pubdir_search_form;

typedef void (*ggp_pubdir_request_cb)(PurpleConnection *gc, int records_count,
	const ggp_pubdir_record *records, int next_offset, void *user_data);

void ggp_pubdir_get_info(PurpleConnection *gc, uin_t uin,
	ggp_pubdir_request_cb cb, void *user_data);

void ggp_pubdir_get_info_protocol(PurpleConnection *gc, const char *name);
void ggp_pubdir_request_buddy_alias(PurpleConnection *gc, PurpleBuddy *buddy);

void ggp_pubdir_search(PurpleConnection *gc,
	const ggp_pubdir_search_form *form);

void ggp_pubdir_set_info(PurpleConnection *gc);

#endif /* _GGP_PUBDIR_PROTOCOL_H */
