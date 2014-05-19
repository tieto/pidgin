/*
 * Image Upload - an inline images implementation for protocols without
 * support for such feature.
 *
 * Copyright (C) 2014, Tomasz Wasilczyk <twasilczyk@pidgin.im>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "internal.h"

#include "debug.h"
#include "glibcompat.h"
#include "version.h"

#include "gtk3compat.h"
#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkutils.h"
#include "gtkwebviewtoolbar.h"
#include "pidginstock.h"

/******************************************************************************
 *
 ******************************************************************************/


/******************************************************************************
 * Plugin setup
 ******************************************************************************/

#if 0
static void
imgup_prpl_init(PurplePlugin *prpl)
{
	PurplePluginProtocolInfo *prpl_info;

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info->options & OPT_PROTO_IM_IMAGE)
		return;

	purple_plugin_set_data(prpl, "imgupload-set", GINT_TO_POINTER(TRUE));
	prpl_info->options |= OPT_PROTO_IM_IMAGE;
}

static void
imgup_prpl_uninit(PurplePlugin *prpl)
{
	PurplePluginProtocolInfo *prpl_info;

	if (!purple_plugin_get_data(prpl, "imgupload-set"))
		return;

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
	prpl_info->options &= ~OPT_PROTO_IM_IMAGE;

	purple_plugin_set_data(prpl, "imgupload-set", NULL);
}
#endif

static gboolean
imgup_plugin_load(PurplePlugin *plugin)
{
#if 0
	GList *it;

	it = purple_plugins_get_protocols();
	for (; it; it = g_list_next(it)) {
		PurplePlugin *prpl = it->data;
		imgup_prpl_init(prpl);
	}
#endif

	return TRUE;
}

static gboolean
imgup_plugin_unload(PurplePlugin *plugin)
{
#if 0
	GList *it;

	it = purple_plugins_get_protocols();
	for (; it; it = g_list_next(it)) {
		PurplePlugin *prpl = it->data;
		imgup_prpl_uninit(prpl);
	}
#endif

	return TRUE;
}

static PurplePluginInfo imgup_info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	PIDGIN_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	"gtk-imgupload",
	N_("Image Upload"),
	DISPLAY_VERSION,
	N_("Inline images implementation for protocols without such feature."),
	N_("Adds inline images support for protocols lacking this feature by "
		"uploading them to the external service."),
	"Tomasz Wasilczyk <twasilczyk@pidgin.im>",
	PURPLE_WEBSITE,
	imgup_plugin_load,
	imgup_plugin_unload,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	/* padding */
	NULL, NULL, NULL, NULL
};

static void
imgup_init_plugin(PurplePlugin *plugin)
{
#if 0
	purple_prefs_add_none("/plugins");
	purple_prefs_add_none("/plugins/gtk");
	purple_prefs_add_none("/plugins/gtk/imgupload");
#endif
}

PURPLE_INIT_PLUGIN(imgupload, imgup_init_plugin, imgup_info)
