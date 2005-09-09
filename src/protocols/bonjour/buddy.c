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
 

#include <glib.h>
#include <stdlib.h>

#include "buddy.h"
#include "account.h"
#include "blist.h"
#include "bonjour.h"
#include "debug.h"

/**
 * Creates a new buddy.
 */
BonjourBuddy* bonjour_buddy_new(gchar* name, gchar* first, gint port_p2pj,
	gchar* phsh, gchar* status, gchar* email, gchar* last, gchar* jid, gchar* AIM,
	gchar* vc, gchar* ip, gchar* msg)
{
	BonjourBuddy* buddy = malloc(sizeof(BonjourBuddy));

	buddy->name = g_strdup(name);
	buddy->first = g_strdup(first);
	buddy->port_p2pj = port_p2pj;
	buddy->phsh = g_strdup(phsh);
	buddy->status = g_strdup(status);
	buddy->email = g_strdup(email);
	buddy->last = g_strdup(last);
	buddy->jid = g_strdup(jid);
	buddy->AIM = g_strdup(AIM);
	buddy->vc = g_strdup(vc);
	buddy->ip = g_strdup(ip);
	buddy->msg = g_strdup(msg);
	buddy->conversation = NULL;
	
	return buddy;
}

/**
 * Check if all the compulsory buddy data is present.
 */
gboolean bonjour_buddy_check(BonjourBuddy* buddy)
{
	if(buddy->name == NULL){
		return FALSE;
	}
	
	if(buddy->first == NULL){
		return FALSE;
	}
	
	if(buddy->last == NULL){
		return FALSE;
	}
	
	if(buddy->port_p2pj == -1){
		return FALSE;
	}
	
	if(buddy->status == NULL){
		return FALSE;
	}
	
	return TRUE;
}

/**
 * If the buddy doesn't previoulsy exists, it is created. Else, its data is changed (???)
 */
void bonjour_buddy_add_to_gaim(BonjourBuddy* buddy, GaimAccount* account)
{
	GaimBuddy* gb = gaim_find_buddy(account, buddy->name);
	GaimGroup* bonjour_group = gaim_find_group(BONJOUR_GROUP_NAME);
	gchar* buddy_alias = NULL;
	gint buddy_status;

	// Create the alias for the buddy using the first and the last name
	buddy_alias = g_strconcat(buddy->first, " ", buddy->last, NULL);

	// Transformation between the bonjour status and Gaim status
	if (g_ascii_strcasecmp("avail", buddy->status) == 0) {
		buddy_status = BONJOUR_STATE_AVAILABLE;
	} else if (g_ascii_strcasecmp("away", buddy->status) == 0) {
		buddy_status = BONJOUR_STATE_AWAY;
	} else if (g_ascii_strcasecmp("dnd", buddy->status) == 0) {
		buddy_status = BONJOUR_STATE_DND;
	} else {
		buddy_status = BONJOUR_STATE_ERROR;
	}
	
	if (gb != NULL) {
		// The buddy already exists
		serv_got_update(account->gc, gb->name, TRUE, gb->evil, gb->signon, gb->idle, buddy_status);
	} else {
		// We have to create the buddy
		gb = gaim_buddy_new(account, buddy->name, buddy_alias);
		gb->node.flags = GAIM_BLIST_NODE_FLAG_NO_SAVE;
		gb->proto_data = buddy;
		gaim_blist_add_buddy(gb, NULL, bonjour_group, NULL);
		gaim_blist_server_alias_buddy(gb, buddy_alias);
		gaim_blist_update_buddy_status(gb, buddy_status);
		gaim_blist_update_buddy_presence(gb, TRUE);
		gaim_blist_update_buddy_signon(gb, 0);
		gaim_blist_update_buddy_idle(gb, 0);
		gaim_blist_update_buddy_evil(gb, 0);
		g_free(buddy_alias);
	}
}

/**
 * Deletes a buddy from memory.
 */
void bonjour_buddy_delete(BonjourBuddy* buddy)
{
	g_free(buddy->name);
	g_free(buddy->first);
	g_free(buddy->phsh);
	g_free(buddy->status);
	g_free(buddy->email);
	g_free(buddy->last);
	g_free(buddy->jid);
	g_free(buddy->AIM);
	g_free(buddy->vc);
	g_free(buddy->ip);
	g_free(buddy->msg);
	
	if (buddy->conversation != NULL) {
		g_free(buddy->conversation->buddy_name);
		g_free(buddy->conversation);
	}
	
	free(buddy);
}
