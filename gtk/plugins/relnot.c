/*
 * Release Notification Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef GAIM_PLUGINS
#define GAIM_PLUGINS
#endif

#include "internal.h"

#include <string.h>

#include "connection.h"
#include "core.h"
#include "notify.h"
#include "prefs.h"
#include "util.h"
#include "version.h"

/* 1 day */
#define MIN_CHECK_INTERVAL 60 * 60 * 24

static void
version_fetch_cb(void *ud, const char *data, size_t len)
{
	const char *changelog = data;
	char *cur_ver, *formatted;
	GString *message;
	int i=0;

	if(!changelog || !len)
		return;

	while(changelog[i] && changelog[i] != '\n') i++;

	cur_ver = g_strndup(changelog, i);
	changelog += i;

	while(*changelog == '\n') changelog++;

	message = g_string_new("");
	g_string_append_printf(message, _("You are using Gaim version %s.  The "
			"current version is %s.<hr>"),
			gaim_core_get_version(), cur_ver);

	if(*changelog) {
		formatted = gaim_strdup_withhtml(changelog);
		g_string_append_printf(message, _("<b>ChangeLog:</b>\n%s<br><br>"),
				formatted);
		g_free(formatted);
	}

	g_string_append_printf(message, _("You can get version %s from:<br>"
			"<a href=\"http://gaim.sourceforge.net/\">"
			"http://gaim.sourceforge.net</a>."), cur_ver);

	gaim_notify_formatted(NULL, _("New Version Available"),
			_("New Version Available"), NULL, message->str,
			NULL, NULL);

	g_string_free(message, TRUE);
	g_free(cur_ver);
}

static void
do_check(void)
{
	int last_check = gaim_prefs_get_int("/plugins/gtk/relnot/last_check");
	if(!last_check || time(NULL) - last_check > MIN_CHECK_INTERVAL) {
		char *url = g_strdup_printf("http://gaim.sourceforge.net/version.php?version=%s&build=%s", gaim_core_get_version(),
#ifdef _WIN32
				"gaim-win32"
#else
				"gaim"
#endif
		);
		gaim_url_fetch(url, TRUE, NULL, FALSE, version_fetch_cb, NULL);
		gaim_prefs_set_int("/plugins/gtk/relnot/last_check", time(NULL));
		g_free(url);
	}
}

static void
signed_on_cb(GaimConnection *gc, void *data) {
	do_check();
}

/**************************************************************************
 * Plugin stuff
 **************************************************************************/
static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						plugin, GAIM_CALLBACK(signed_on_cb), NULL);

	/* we don't check if we're offline */
	if(gaim_connections_get_all())
		do_check();

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"gtk-relnot",                                     /**< id             */
	N_("Release Notification"),                       /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Checks periodically for new releases."),
	                                                  /**  description    */
	N_("Checks periodically for new releases and notifies the user "
			"with the ChangeLog."),
	"Nathan Walp <faceprint@faceprint.com>",          /**< author         */
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
	gaim_prefs_add_none("/plugins/gtk/relnot");
	gaim_prefs_add_int("/plugins/gtk/relnot/last_check", 0);
}

GAIM_INIT_PLUGIN(relnot, init_plugin, info)
