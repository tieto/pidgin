/* purple
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
#include "internal.h"
#include "pidgin.h"

#include "conversation.h"
#include "signals.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkplugin.h"

#define ICONAWAY_PLUGIN_ID "gtk-iconaway"

static void
iconify_windows(PurpleAccount *account, PurpleStatus *old, PurpleStatus *newstatus)
{
	PurplePresence *presence;
	PidginWindow *win;
	GList *windows;

	presence = purple_status_get_presence(newstatus);

	if (purple_presence_is_available(presence))
		return;

	purple_blist_set_visible(FALSE);

	for (windows = pidgin_conv_windows_get_list();
		 windows != NULL;
		 windows = windows->next) {

		win = (PidginWindow *)windows->data;

		gtk_window_iconify(GTK_WINDOW(win->window));
	}
}

/*
 *  EXPORTED FUNCTIONS
 */

static PidginPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Eric Warmenhoven <eric@warmenhoven.org>",
		NULL
	};

	return pidgin_plugin_info_new(
		"id",           ICONAWAY_PLUGIN_ID,
		"name",         N_("Iconify on Away"),
		"version",      DISPLAY_VERSION,
		"category",     N_("User interface"),
		"summary",      N_("Iconifies the buddy list and your conversations "
		                   "when you go away."),
		"description",  N_("Iconifies the buddy list and your conversations "
		                   "when you go away."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	purple_signal_connect(purple_accounts_get_handle(), "account-status-changed",
						plugin, PURPLE_CALLBACK(iconify_windows), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT(iconaway, plugin_query, plugin_load, plugin_unload);
