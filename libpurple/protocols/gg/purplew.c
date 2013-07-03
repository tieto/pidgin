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

#include "purplew.h"

#include <request.h>
#include <debug.h>

guint ggp_purplew_http_input_add(struct gg_http *http_req,
	PurpleInputFunction func, gpointer user_data)
{
	PurpleInputCondition cond = 0;
	int check = http_req->check;

	if (check & GG_CHECK_READ)
		cond |= PURPLE_INPUT_READ;
	if (check & GG_CHECK_WRITE)
		cond |= PURPLE_INPUT_WRITE;

	//TODO: verbose mode
	//purple_debug_misc("gg", "ggp_purplew_http_input_add: "
	//	"[req=%x, fd=%d, cond=%d]\n",
	//	(unsigned int)http_req, http_req->fd, cond);
	return purple_input_add(http_req->fd, cond, func, user_data);
}

static void ggp_purplew_request_processing_cancel(
	ggp_purplew_request_processing_handle *handle, gint id)
{
	handle->cancel_cb(handle->gc, handle->user_data);
	g_free(handle);
}

ggp_purplew_request_processing_handle * ggp_purplew_request_processing(
	PurpleConnection *gc, const gchar *msg, void *user_data,
	ggp_purplew_request_processing_cancel_cb cancel_cb)
{
	ggp_purplew_request_processing_handle *handle =
		g_new(ggp_purplew_request_processing_handle, 1);

	handle->gc = gc;
	handle->cancel_cb = cancel_cb;
	handle->user_data = user_data;
	handle->request_handle = purple_request_action(gc, _("Please wait..."),
		(msg ? msg : _("Please wait...")), NULL,
		PURPLE_DEFAULT_ACTION_NONE, purple_connection_get_account(gc),
		NULL, NULL, handle, 1,
		_("Cancel"), G_CALLBACK(ggp_purplew_request_processing_cancel));
	
	return handle;
}

void ggp_purplew_request_processing_done(
	ggp_purplew_request_processing_handle *handle)
{
	purple_request_close(PURPLE_REQUEST_ACTION, handle->request_handle);
	g_free(handle);
}

PurpleGroup * ggp_purplew_buddy_get_group_only(PurpleBuddy *buddy)
{
	PurpleGroup *group = purple_buddy_get_group(buddy);
	if (!group)
		return NULL;
	if (0 == strcmp(GGP_PURPLEW_GROUP_DEFAULT, purple_group_get_name(group)))
		return NULL;
	return group;
}

GList * ggp_purplew_group_get_buddies(PurpleGroup *group, PurpleAccount *account)
{
	GList *buddies = NULL;
	PurpleBlistNode *gnode, *cnode, *bnode;
	
	g_return_val_if_fail(group != NULL, NULL);
	
	gnode = PURPLE_BLIST_NODE(group);
	for (cnode = purple_blist_node_get_first_child(gnode);
		cnode != NULL;
		cnode = purple_blist_node_get_sibling_next(cnode))
	{
		if (!PURPLE_IS_CONTACT(cnode))
			continue;
		for (bnode = purple_blist_node_get_first_child(cnode);
			bnode != NULL;
			bnode = purple_blist_node_get_sibling_next(bnode))
		{
			PurpleBuddy *buddy;
			if (!PURPLE_IS_BUDDY(bnode))
				continue;
			
			buddy = PURPLE_BUDDY(bnode);
			if (account == NULL || purple_buddy_get_account(buddy) == account)
				buddies = g_list_append(buddies, buddy);
		}
	}
	
	return buddies;
}

GList * ggp_purplew_account_get_groups(PurpleAccount *account, gboolean exclusive)
{
	PurpleBlistNode *bnode;
	GList *groups = NULL;
	for (bnode = purple_blist_get_root();
		bnode != NULL;
		bnode = purple_blist_node_get_sibling_next(bnode))
	{
		PurpleGroup *group;
		GSList *accounts;
		gboolean have_specified = FALSE, have_others = FALSE;
		
		if (!PURPLE_IS_GROUP(bnode))
			continue;
		
		group = PURPLE_GROUP(bnode);
		for (accounts = purple_group_get_accounts(group); accounts; accounts = g_slist_delete_link(accounts, accounts))
		{
			if (accounts->data == account)
				have_specified = TRUE;
			else
				have_others = TRUE;
		}
		
		if (have_specified && (!exclusive || !have_others))
			groups = g_list_append(groups, group);
	}
	return groups;
}
