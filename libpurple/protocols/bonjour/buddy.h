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

#include <glib.h>

#include "config.h"
#include "account.h"
#include "jabber.h"

#ifdef USE_BONJOUR_APPLE 
#include "dns_sd_proxy.h"
#else /* USE_BONJOUR_HOWL */
#include <howl.h>
#endif

typedef struct _BonjourBuddy
{
	PurpleAccount *account;

	gchar *name;
	gchar *ip;
	gint port_p2pj;

	gchar *first;
	gchar *phsh;
	gchar *status;
	gchar *email;
	gchar *last;
	gchar *jid;
	gchar *AIM;
	gchar *vc;
	gchar *msg;
	gchar *ext;
	gchar *nick;
	gchar *node;
	gchar *ver;

	BonjourJabberConversation *conversation;

#ifdef USE_BONJOUR_APPLE
	DNSServiceRef txt_query;
	int txt_query_fd;
#endif

} BonjourBuddy;

static const char *const buddy_TXT_records[] = {
	"1st",
	"email",
	"ext",
	"jid",
	"last",
	"msg",
	"nick",
	"node",
	"phsh",
/*	"port.p2pj", Deprecated - MUST ignore */
	"status",
/*	"txtvers", Deprecated - hardcoded to 1 */
	"vc",
	"ver",
	"AIM", /* non standard */
	NULL
};

/**
 * Creates a new buddy.
 */
BonjourBuddy *bonjour_buddy_new(const gchar *name, PurpleAccount *account);

/**
 * Sets a value in the BonjourBuddy struct, destroying the old value
 */
void set_bonjour_buddy_value(BonjourBuddy* buddy, const char *record_key, const char *value, uint32_t len);

/**
 * Check if all the compulsory buddy data is present.
 */
gboolean bonjour_buddy_check(BonjourBuddy *buddy);

/**
 * If the buddy doesn't previoulsy exists, it is created. Else, its data is changed (???)
 */
void bonjour_buddy_add_to_purple(BonjourBuddy *buddy);

/**
 * Deletes a buddy from memory.
 */
void bonjour_buddy_delete(BonjourBuddy *buddy);

#endif
