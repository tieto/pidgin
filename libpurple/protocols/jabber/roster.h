/**
 * @file roster.h Roster manipulation
 *
 * purple
 *
 * Copyright (C) 2003 Nathan Walp <faceprint@faceprint.com>
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
#ifndef _PURPLE_JABBER_ROSTER_H_
#define _PURPLE_JABBER_ROSTER_H_

#include "jabber.h"

void jabber_roster_request(JabberStream *js);

void jabber_roster_parse(JabberStream *js, xmlnode *packet);

void jabber_roster_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
		PurpleGroup *group);
void jabber_roster_alias_change(PurpleConnection *gc, const char *name,
		const char *alias);
void jabber_roster_group_change(PurpleConnection *gc, const char *name,
		const char *old_group, const char *new_group);
void jabber_roster_group_rename(PurpleConnection *gc, const char *old_name,
		PurpleGroup *group, GList *moved_buddies);
void jabber_roster_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
		PurpleGroup *group);

#endif /* _PURPLE_JABBER_ROSTER_H_ */
