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

#ifndef _GGP_ROSTER_H
#define _GGP_ROSTER_H

#include <internal.h>
#include <libgadu.h>

typedef struct
{
	gpointer content;
	gboolean is_updating;
	
	GList *sent_updates;
	GList *pending_updates;
	
	guint timer;
} ggp_roster_session_data;

// setup
gboolean ggp_roster_enabled(void);
void ggp_roster_setup(PurpleConnection *gc);
void ggp_roster_cleanup(PurpleConnection *gc);

// synchronization control
void ggp_roster_request_update(PurpleConnection *gc);

// libgadu callbacks
void ggp_roster_reply(PurpleConnection *gc,
	struct gg_event_userlist100_reply *reply);
void ggp_roster_version(PurpleConnection *gc,
	struct gg_event_userlist100_version *version);

// libpurple callbacks
void ggp_roster_alias_buddy(PurpleConnection *gc, const char *who,
	const char *alias);
void ggp_roster_group_buddy(PurpleConnection *gc, const char *who,
	const char *old_group, const char *new_group);
void ggp_roster_rename_group(PurpleConnection *, const char *old_name,
	PurpleGroup *group, GList *moved_buddies);
void ggp_roster_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
	PurpleGroup *group, const char *message);
void ggp_roster_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
	PurpleGroup *group);

#endif /* _GGP_ROSTER_H */
