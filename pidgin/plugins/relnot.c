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

#ifndef PURPLE_PLUGINS
#define PURPLE_PLUGINS
#endif

#include "internal.h"

#include <string.h>

#include "connection.h"
#include "core.h"
#include "notify.h"
#include "prefs.h"
#include "util.h"
#include "version.h"

#include "pidgin.h"

/* 1 day */
#define MIN_CHECK_INTERVAL 60 * 60 * 24

static void
version_fetch_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data,
		const gchar *changelog, size_t len, const gchar *error_message)
{
	char *cur_ver, *formatted;
	GString *message;
	int i=0;

	if(error_message || !changelog || !len)
		return;

	while(changelog[i] && changelog[i] != '\n') i++;

	/* this basically means the version thing wasn't in the format we were
	 * looking for so sourceforge is probably having web server issues, and
	 * we should try again later */
	if(i == 0)
		return;

	cur_ver = g_strndup(changelog, i);
	changelog += i;

	while(*changelog == '\n') changelog++;

	message = g_string_new("");
	g_string_append_printf(message, _("You are using " PIDGIN_NAME " version %s.  The "
			"current version is %s.<hr>"),
			purple_core_get_version(), cur_ver);

	if(*changelog) {
		formatted = purple_strdup_withhtml(changelog);
		g_string_append_printf(message, _("<b>ChangeLog:</b>\n%s<br><br>"),
				formatted);
		g_free(formatted);
	}

	g_string_append_printf(message, _("You can get version %s from:<br>"
			"<a href=\"http://pidgin.im/\">"
			"http://pidgin.im</a>."), cur_ver);

	purple_notify_formatted(NULL, _("New Version Available"),
			_("New Version Available"), NULL, message->str,
			NULL, NULL);

	g_string_free(message, TRUE);
	g_free(cur_ver);
}

static void
do_check(void)
{
	int last_check = purple_prefs_get_int("/plugins/gtk/relnot/last_check");
	if(!last_check || time(NULL) - last_check > MIN_CHECK_INTERVAL) {
		char *url = g_strdup_printf("http://purple.sourceforge.net/version.php?version=%s&build=%s", purple_core_get_version(),
#ifdef _WIN32
				"purple-win32"
#else
				"purple"
#endif
		);
		purple_util_fetch_url(url, TRUE, NULL, FALSE, version_fetch_cb, NULL);
		purple_prefs_set_int("/plugins/gtk/relnot/last_check", time(NULL));
		g_free(url);
	}
}

static void
signed_on_cb(PurpleConnection *gc, void *data) {
	do_check();
}

/**************************************************************************
 * Plugin stuff
 **************************************************************************/
static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(purple_connections_get_handle(), "signed-on",
						plugin, PURPLE_CALLBACK(signed_on_cb), NULL);

	/* we don't check if we're offline */
	if(purple_connections_get_all())
		do_check();

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	"gtk-relnot",                                     /**< id             */
	N_("Release Notification"),                       /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Checks periodically for new releases."),
	                                                  /**  description    */
	N_("Checks periodically for new releases and notifies the user "
			"with the ChangeLog."),
	"Nathan Walp <faceprint@faceprint.com>",          /**< author         */
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
	purple_prefs_add_none("/plugins/gtk/relnot");
	purple_prefs_add_int("/plugins/gtk/relnot/last_check", 0);
}

PURPLE_INIT_PLUGIN(relnot, init_plugin, info)
