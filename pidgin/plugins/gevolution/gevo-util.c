/*
 * Evolution integration plugin for Purple
 *
 * Copyright (C) 2003 Christian Hammond.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */
#include "internal.h"
#include "gtkblist.h"
#include "pidgin.h"
#include "gtkutils.h"

#include "gevolution.h"

void
gevo_add_buddy(PurpleAccount *account, const char *group_name,
			   const char *buddy_name, const char *alias)
{
	PurpleIMConversation *im = NULL;
	PurpleBuddy *buddy;
	PurpleGroup *group;

	im = purple_conversations_find_im_with_account(buddy_name, account);

	group = purple_blist_find_group(group_name);
	if (group == NULL)
	{
		group = purple_group_new(group_name);
		purple_blist_add_group(group, NULL);
	}

	buddy = purple_blist_find_buddy_in_group(account, buddy_name, group);
	if (buddy == NULL)
	{
		buddy = purple_buddy_new(account, buddy_name, alias);
		purple_blist_add_buddy(buddy, NULL, group, NULL);
	}

	purple_account_add_buddy(account, buddy, NULL);

	if (im != NULL)
	{
		purple_buddy_icon_update(purple_im_conversation_get_icon(im));
		purple_conversation_update(PURPLE_CONVERSATION(im),
			PURPLE_CONVERSATION_UPDATE_ADD);
	}
}

GList *
gevo_get_groups(void)
{
	static GList *list = NULL;
	PurpleGroup *g;
	PurpleBlistNode *gnode;

	g_list_free(list);
	list = NULL;

	if (purple_blist_get_buddy_list()->root == NULL) {
		list  = g_list_append(list,
			(gpointer)PURPLE_BLIST_DEFAULT_GROUP_NAME);
	} else {
		for (gnode = purple_blist_get_buddy_list()->root;
			 gnode != NULL;
			 gnode = gnode->next)
		{
			if (PURPLE_IS_GROUP(gnode))
			{
				g = PURPLE_GROUP(gnode);
				list = g_list_append(list,
					(gpointer)purple_group_get_name(g));
			}
		}
	}

	return list;
}

EContactField
gevo_protocol_get_field(PurpleAccount *account, PurpleBuddy *buddy)
{
	EContactField protocol_field = 0;
	const char *protocol_id;

	g_return_val_if_fail(account != NULL, 0);

	protocol_id = purple_account_get_protocol_id(account);

	if (!strcmp(protocol_id, "prpl-aim"))
		protocol_field = E_CONTACT_IM_AIM;
	else if (!strcmp(protocol_id, "prpl-icq"))
		protocol_field = E_CONTACT_IM_ICQ;
	else if (!strcmp(protocol_id, "prpl-msn"))
		protocol_field = E_CONTACT_IM_MSN;
	else if (!strcmp(protocol_id, "prpl-yahoo"))
		protocol_field = E_CONTACT_IM_YAHOO;
	else if (!strcmp(protocol_id, "prpl-jabber"))
		protocol_field = E_CONTACT_IM_JABBER;
	else if (!strcmp(protocol_id, "prpl-novell"))
		protocol_field = E_CONTACT_IM_GROUPWISE;
	else if (!strcmp(protocol_id, "prpl-gg"))
		protocol_field = E_CONTACT_IM_GADUGADU;

	return protocol_field;
}

gboolean
gevo_protocol_is_supported(PurpleAccount *account, PurpleBuddy *buddy)
{
	return (gevo_protocol_get_field(account, buddy) != 0);
}

gboolean
gevo_load_addressbook(const gchar* uid, EBook **book, GError **error)
{
	gboolean result = FALSE;
	ESourceRegistry *registry;
	ESource *source;

	g_return_val_if_fail(book != NULL, FALSE);

	registry = e_source_registry_new_sync(NULL, error);

	if (!registry)
		return FALSE;

	if (uid == NULL)
		source = e_source_registry_ref_default_address_book(registry);
	else
		source = e_source_registry_ref_source(registry, uid);

	g_object_unref(registry);

	result = gevo_load_addressbook_from_source(source, book, error);

	g_object_unref(source);

	return result;
}

gboolean
gevo_load_addressbook_from_source(ESource *source, EBook **book, GError **error)
{
	gboolean result = FALSE;

	*book = e_book_new(source, error);

	if (*book == NULL)
		return FALSE;

	*error = NULL;

	result = e_book_open(*book, FALSE, error);

	if (!result && *book != NULL)
	{
		g_object_unref(*book);
		*book = NULL;
	}

	return result;
}

char *
gevo_get_email_for_buddy(PurpleBuddy *buddy)
{
	EContact *contact;
	char *mail = NULL;

	contact = gevo_search_buddy_in_contacts(buddy, NULL);

	if (contact != NULL)
	{
		mail = g_strdup(e_contact_get(contact, E_CONTACT_EMAIL_1));
		g_object_unref(contact);
	}

	if (mail == NULL)
	{
		PurpleAccount *account = purple_buddy_get_account(buddy);
		const char *protocol_id = purple_account_get_protocol_id(account);

		if (!strcmp(protocol_id, "prpl-msn"))
		{
			mail = g_strdup(purple_normalize(account,
										   purple_buddy_get_name(buddy)));
		}
		else if (!strcmp(protocol_id, "prpl-yahoo"))
		{
			mail = g_strdup_printf("%s@yahoo.com",
								   purple_normalize(account,
												  purple_buddy_get_name(buddy)));
		}
	}

	return mail;
}
