/* gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
iconify_windows(GaimAccount *account, GaimStatus *old, GaimStatus *newstatus)
{
	GaimPresence *presence;
	PidginWindow *win;
	GList *windows;

	presence = gaim_status_get_presence(newstatus);

	if (gaim_presence_is_available(presence))
		return;

	gaim_blist_set_visible(FALSE);

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
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(gaim_accounts_get_handle(), "account-status-changed",
						plugin, GAIM_CALLBACK(iconify_windows), NULL);

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	ICONAWAY_PLUGIN_ID,                               /**< id             */
	N_("Iconify on Away"),                            /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Iconifies the buddy list and your conversations when you go away."),
	                                                  /**  description    */
	N_("Iconifies the buddy list and your conversations when you go away."),
	"Eric Warmenhoven <eric@warmenhoven.org>",        /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(iconaway, init_plugin, info)
