/**
 * @file presence.h Presence
 *
 * gaim
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
#ifndef _GAIM_JABBER_PRESENCE_H_
#define _GAIM_JABBER_PRESENCE_H_

#include "jabber.h"
#include "xmlnode.h"

#define JABBER_STATE_AWAY  (0x02 | UC_UNAVAILABLE)
#define JABBER_STATE_CHAT  (0x04)
#define JABBER_STATE_XA    (0x08 | UC_UNAVAILABLE)
#define JABBER_STATE_DND   (0x10 | UC_UNAVAILABLE)
#define JABBER_STATE_ERROR (0x20 | UC_UNAVAILABLE)

void jabber_presence_send(GaimConnection *gc, const char *state,
		const char *msg);
xmlnode *jabber_presence_create(GaimStatus *status);
void jabber_presence_parse(JabberStream *js, xmlnode *packet);
void jabber_presence_subscription_set(JabberStream *js, const char *who,
		const char *type);
void jabber_presence_fake_to_self(JabberStream *js, const char *away_state, const char *msg);

#endif /* _GAIM_JABBER_PRESENCE_H_ */
