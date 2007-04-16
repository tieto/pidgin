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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(purple_accounts_get_handle(), "account-status-changed",
						plugin, PURPLE_CALLBACK(iconify_windows), NULL);

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	ICONAWAY_PLUGIN_ID,                               /**< id             */
	N_("Iconify on Away"),                            /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Iconifies the buddy list and your conversations when you go away."),
	                                                  /**  description    */
	N_("Iconifies the buddy list and your conversations when you go away."),
	"Eric Warmenhoven <eric@warmenhoven.org>",        /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(iconaway, init_plugin, info)
