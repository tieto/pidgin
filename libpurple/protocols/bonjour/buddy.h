/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _BONJOUR_BUDDY
#define _BONJOUR_BUDDY

#include <howl.h>
#include <glib.h>

#include "account.h"
#include "jabber.h"

typedef struct _BonjourBuddy
{
	gchar *name;
	gchar *first;
	gint port_p2pj;
	gchar *phsh;
	gchar *status;
	gchar *email;
	gchar *last;
	gchar *jid;
	gchar *AIM;
	gchar *vc;
	gchar *ip;
	gchar *msg;
	BonjourJabberConversation *conversation;
} BonjourBuddy;

/**
 * Creates a new buddy.
 */
BonjourBuddy *bonjour_buddy_new(const gchar *name, const gchar *first,
	gint port_p2pj, const gchar *phsh, const gchar *status,
	const gchar *email, const gchar *last, const gchar *jid,
	const gchar *AIM, const gchar *vc, const gchar *ip, const gchar *msg);

/**
 * Check if all the compulsory buddy data is present.
 */
gboolean bonjour_buddy_check(BonjourBuddy *buddy);

/**
 * If the buddy doesn't previoulsy exists, it is created. Else, its data is changed (???)
 */
void bonjour_buddy_add_to_purple(PurpleAccount *account, BonjourBuddy *buddy);

/**
 * Deletes a buddy from memory.
 */
void bonjour_buddy_delete(BonjourBuddy *buddy);

#endif
