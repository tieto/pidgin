/**
 * @file accounts.c Accounts API
 * @ingroup core
 */

/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#include "accounts.h"

void
purple_accounts_add(PurpleAccount *account)
{
	g_return_if_fail(account != NULL);

	if (g_list_find(accounts, account) != NULL)
		return;

	accounts = g_list_append(accounts, account);

	schedule_accounts_save();

	purple_signal_emit(purple_accounts_get_handle(), "account-added", account);
}

void
purple_accounts_remove(PurpleAccount *account)
{
	g_return_if_fail(account != NULL);

	accounts = g_list_remove(accounts, account);

	schedule_accounts_save();

	/* Clearing the error ensures that account-error-changed is emitted,
	 * which is the end of the guarantee that the the error's pointer is
	 * valid.
	 */
	purple_account_clear_current_error(account);
	purple_signal_emit(purple_accounts_get_handle(), "account-removed", account);
}

static void
purple_accounts_delete_set(PurpleAccount *account, GError *error, gpointer data)
{
	purple_account_destroy(account);
}

void
purple_accounts_delete(PurpleAccount *account)
{
	PurpleBlistNode *gnode, *cnode, *bnode;
	GList *iter;

	g_return_if_fail(account != NULL);

	/*
	 * Disable the account before blowing it out of the water.
	 * Conceptually it probably makes more sense to disable the
	 * account for all UIs rather than the just the current UI,
	 * but it doesn't really matter.
	 */
	purple_account_set_enabled(account, purple_core_get_ui(), FALSE);

	purple_notify_close_with_handle(account);
	purple_request_close_with_handle(account);

	purple_accounts_remove(account);

	/* Remove this account's buddies */
	for (gnode = purple_blist_get_root();
	     gnode != NULL;
		 gnode = purple_blist_node_get_sibling_next(gnode))
	{
		if (!PURPLE_BLIST_NODE_IS_GROUP(gnode))
			continue;

		cnode = purple_blist_node_get_first_child(gnode);
		while (cnode) {
			PurpleBlistNode *cnode_next = purple_blist_node_get_sibling_next(cnode);

			if(PURPLE_BLIST_NODE_IS_CONTACT(cnode)) {
				bnode = purple_blist_node_get_first_child(cnode);
				while (bnode) {
					PurpleBlistNode *bnode_next = purple_blist_node_get_sibling_next(bnode);

					if (PURPLE_BLIST_NODE_IS_BUDDY(bnode)) {
						PurpleBuddy *b = (PurpleBuddy *)bnode;

						if (purple_buddy_get_account(b) == account)
							purple_blist_remove_buddy(b);
					}
					bnode = bnode_next;
				}
			} else if (PURPLE_BLIST_NODE_IS_CHAT(cnode)) {
				PurpleChat *c = (PurpleChat *)cnode;

				if (purple_chat_get_account(c) == account)
					purple_blist_remove_chat(c);
			}
			cnode = cnode_next;
		}
	}

	/* Remove any open conversation for this account */
	for (iter = purple_get_conversations(); iter; ) {
		PurpleConversation *conv = iter->data;
		iter = iter->next;
		if (purple_conversation_get_account(conv) == account)
			purple_conversation_destroy(conv);
	}

	/* Remove this account's pounces */
	purple_pounce_destroy_all_by_account(account);

	/* This will cause the deletion of an old buddy icon. */
	purple_buddy_icons_set_account_icon(account, NULL, 0);

	/* This is async because we do not want the
	 * account being overwritten before we are done.
	 */
	purple_keyring_set_password(account, NULL,
		purple_accounts_delete_set, NULL);
}

void
purple_accounts_reorder(PurpleAccount *account, gint new_index)
{
	gint index;
	GList *l;

	g_return_if_fail(account != NULL);
	g_return_if_fail(new_index <= g_list_length(accounts));

	index = g_list_index(accounts, account);

	if (index == -1) {
		purple_debug_error("account",
				   "Unregistered account (%s) discovered during reorder!\n",
				   purple_account_get_username(account));
		return;
	}

	l = g_list_nth(accounts, index);

	if (new_index > index)
		new_index--;

	/* Remove the old one. */
	accounts = g_list_delete_link(accounts, l);

	/* Insert it where it should go. */
	accounts = g_list_insert(accounts, account, new_index);

	schedule_accounts_save();
}

GList *
purple_accounts_get_all(void)
{
	return accounts;
}

GList *
purple_accounts_get_all_active(void)
{
	GList *list = NULL;
	GList *all = purple_accounts_get_all();

	while (all != NULL) {
		PurpleAccount *account = all->data;

		if (purple_account_get_enabled(account, purple_core_get_ui()))
			list = g_list_append(list, account);

		all = all->next;
	}

	return list;
}

PurpleAccount *
purple_accounts_find(const char *name, const char *protocol_id)
{
	PurpleAccount *account = NULL;
	GList *l;
	char *who;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(protocol_id != NULL, NULL);

	for (l = purple_accounts_get_all(); l != NULL; l = l->next) {
		account = (PurpleAccount *)l->data;
		if (!purple_strequal(account->protocol_id, protocol_id))
			continue;

		who = g_strdup(purple_normalize(account, name));
		if (purple_strequal(purple_normalize(account, purple_account_get_username(account)), who)) {
			g_free(who);
			return account;
		}
		g_free(who);
	}

	return NULL;
}

void
purple_accounts_restore_current_statuses()
{
	GList *l;
	PurpleAccount *account;

	/* If we're not connected to the Internet right now, we bail on this */
	if (!purple_network_is_available())
	{
		purple_debug_warning("account", "Network not connected; skipping reconnect\n");
		return;
	}

	for (l = purple_accounts_get_all(); l != NULL; l = l->next)
	{
		account = (PurpleAccount *)l->data;
		if (purple_account_get_enabled(account, purple_core_get_ui()) &&
			(purple_presence_is_online(account->presence)))
		{
			purple_account_connect(account);
		}
	}
}

void
purple_accounts_set_ui_ops(PurpleAccountUiOps *ops)
{
	account_ui_ops = ops;
}

PurpleAccountUiOps *
purple_accounts_get_ui_ops(void)
{
	return account_ui_ops;
}

void *
purple_accounts_get_handle(void)
{
	static int handle;

	return &handle;
}

void
purple_accounts_init(void)
{
	void *handle = purple_accounts_get_handle();
	void *conn_handle = purple_connections_get_handle();

	purple_signal_register(handle, "account-connecting",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-disabled",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-enabled",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-setting-info",
						 purple_marshal_VOID__POINTER_POINTER, NULL, 2,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "account-set-info",
						 purple_marshal_VOID__POINTER_POINTER, NULL, 2,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "account-created",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-destroying",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-added",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-removed",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-status-changed",
						 purple_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_STATUS),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_STATUS));

	purple_signal_register(handle, "account-actions-changed",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-alias-changed",
						 purple_marshal_VOID__POINTER_POINTER, NULL, 2,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
							 			PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "account-authorization-requested",
						purple_marshal_INT__POINTER_POINTER_POINTER,
						purple_value_new(PURPLE_TYPE_INT), 4,
						purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						purple_value_new(PURPLE_TYPE_STRING),
						purple_value_new(PURPLE_TYPE_STRING),
						purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "account-authorization-denied",
						purple_marshal_VOID__POINTER_POINTER, NULL, 3,
						purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						purple_value_new(PURPLE_TYPE_STRING),
						purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "account-authorization-granted",
						purple_marshal_VOID__POINTER_POINTER, NULL, 3,
						purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						purple_value_new(PURPLE_TYPE_STRING),
						purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "account-error-changed",
	                       purple_marshal_VOID__POINTER_POINTER_POINTER,
	                       NULL, 3,
	                       purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                        PURPLE_SUBTYPE_ACCOUNT),
	                       purple_value_new(PURPLE_TYPE_POINTER),
	                       purple_value_new(PURPLE_TYPE_POINTER));

	purple_signal_register(handle, "account-signed-on",
	                       purple_marshal_VOID__POINTER, NULL, 1,
	                       purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                        PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-signed-off",
	                       purple_marshal_VOID__POINTER, NULL, 1,
	                       purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                        PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-connection-error",
	                       purple_marshal_VOID__POINTER_INT_POINTER, NULL, 3,
	                       purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                        PURPLE_SUBTYPE_ACCOUNT),
	                       purple_value_new(PURPLE_TYPE_ENUM),
	                       purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_connect(conn_handle, "signed-on", handle,
	                      PURPLE_CALLBACK(signed_on_cb), NULL);
	purple_signal_connect(conn_handle, "signed-off", handle,
	                      PURPLE_CALLBACK(signed_off_cb), NULL);
	purple_signal_connect(conn_handle, "connection-error", handle,
	                      PURPLE_CALLBACK(connection_error_cb), NULL);
	purple_signal_connect(purple_keyring_get_handle(), "password-migration", handle,
	                      PURPLE_CALLBACK(password_migration_cb), NULL);

	load_accounts();

}

void
purple_accounts_uninit(void)
{
	gpointer handle = purple_accounts_get_handle();
	if (save_timer != 0)
	{
		purple_timeout_remove(save_timer);
		save_timer = 0;
		sync_accounts();
	}

	for (; accounts; accounts = g_list_delete_link(accounts, accounts))
		purple_account_destroy(accounts->data);

	purple_signals_disconnect_by_handle(handle);
	purple_signals_unregister_by_instance(handle);
}
