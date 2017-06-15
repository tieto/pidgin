/*
 * One Time Password support plugin for libpurple
 *
 * Copyright (C) 2009, Daniel Atallah <datallah@pidgin.im>
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
#include "debug.h"
#include "plugins.h"
#include "version.h"
#include "account.h"
#include "accountopt.h"

#define PLUGIN_ID "core-one_time_password"
#define PREF_NAME PLUGIN_ID "_enabled"

static void
signed_on_cb(PurpleConnection *conn, void *data)
{
	PurpleAccount *account = purple_connection_get_account(conn);

	if (purple_account_get_bool(account, PREF_NAME, FALSE)) {
		if(purple_account_get_remember_password(account))
			purple_debug_error("One Time Password",
					   "Unable to enforce one time password for account %s (%s).\n"
					   "Account is set to remember the password.\n",
					   purple_account_get_username(account),
					   purple_account_get_protocol_name(account));
		else {

			purple_debug_info("One Time Password", "Clearing password for account %s (%s).\n",
					  purple_account_get_username(account),
					  purple_account_get_protocol_name(account));

			purple_account_set_password(account, NULL, NULL, NULL);
			/* TODO: Do we need to somehow clear conn->password ? */
		}
	}
}

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Daniel Atallah <datallah@pidgin.im>",
		NULL
	};

	return purple_plugin_info_new(
		"id",           PLUGIN_ID,
		"name",         N_("One Time Password Support"),
		"version",      DISPLAY_VERSION,
		"category",     N_("Security"),
		"summary",      N_("Enforce that passwords are used only once."),
		"description",  N_("Allows you to enforce on a per-account basis that "
		                   "passwords not being saved are only used in a "
		                   "single successful connection.\n"
		                   "Note: The account password must not be saved for "
		                   "this to work."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	PurpleProtocol *protocol;
	PurpleAccountOption *option;
	GList *list, *l;

	list = purple_protocols_get_all();

	/* Register protocol preference. */
	for (l = list; l != NULL; l = l->next) {
		protocol = PURPLE_PROTOCOL(l->data);
		if (protocol != NULL && !(purple_protocol_get_options(protocol) & OPT_PROTO_NO_PASSWORD)) {
			option = purple_account_option_bool_new(_("One Time Password"),
								PREF_NAME, FALSE);
			protocol->account_options = g_list_append(protocol->account_options, option);
		}
	}
	g_list_free(list);

	/* Register callback. */
	purple_signal_connect(purple_connections_get_handle(), "signed-on",
			      plugin, PURPLE_CALLBACK(signed_on_cb), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	PurpleProtocol *protocol;
	PurpleAccountOption *option;
	GList *list, *l, *options;

	list = purple_protocols_get_all();

	/* Remove protocol preference. */
	for (l = list; l != NULL; l = l->next) {
		protocol = PURPLE_PROTOCOL(l->data);
		if (protocol != NULL && !(purple_protocol_get_options(protocol) & OPT_PROTO_NO_PASSWORD)) {
			options = purple_protocol_get_account_options(protocol);
			while (options != NULL) {
				option = (PurpleAccountOption *) options->data;
				if (purple_strequal(PREF_NAME, purple_account_option_get_setting(option))) {
					protocol->account_options = g_list_delete_link(protocol->account_options, options);
					purple_account_option_destroy(option);
					break;
				}
				options = options->next;
			}
		}
	}
	g_list_free(list);

	/* Callback will be automagically unregistered */

	return TRUE;
}

PURPLE_PLUGIN_INIT(one_time_password, plugin_query, plugin_load, plugin_unload);
