/*
 * Extra conversation placement options for Purple
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301, USA.
 */

#include "internal.h"
#include "pidgin.h"
#include "conversation.h"
#include "version.h"
#include "gtkplugin.h"
#include "gtkconv.h"
#include "gtkconvwin.h"

static void
conv_placement_by_number(PidginConversation *conv)
{
	PidginWindow *win = NULL;
	GList *wins = NULL;

	if (purple_prefs_get_bool("/plugins/gtk/extplacement/placement_number_separate"))
		win = PURPLE_IS_IM_CONVERSATION(conv->active_conv) ?
				pidgin_conv_window_last_im() : pidgin_conv_window_last_chat();
	else if ((wins = pidgin_conv_windows_get_list()) != NULL)
		win = g_list_last(wins)->data;

	if (win == NULL) {
		win = pidgin_conv_window_new();

		pidgin_conv_window_add_gtkconv(win, conv);
		pidgin_conv_window_show(win);
	} else {
		int max_count = purple_prefs_get_int("/plugins/gtk/extplacement/placement_number");
		int count = pidgin_conv_window_get_gtkconv_count(win);

		if (count < max_count)
			pidgin_conv_window_add_gtkconv(win, conv);
		else {
			GList *l = NULL;

			for (l = pidgin_conv_windows_get_list(); l != NULL; l = l->next) {
				win = l->data;

				if (purple_prefs_get_bool("/plugins/gtk/extplacement/placement_number_separate") &&
					G_TYPE_FROM_INSTANCE(pidgin_conv_window_get_active_conversation(win)) != G_TYPE_FROM_INSTANCE(conv->active_conv))
					continue;

				count = pidgin_conv_window_get_gtkconv_count(win);
				if (count < max_count) {
					pidgin_conv_window_add_gtkconv(win, conv);
					return;
				}
			}
			win = pidgin_conv_window_new();

			pidgin_conv_window_add_gtkconv(win, conv);
			pidgin_conv_window_show(win);
		}
	}
}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin) {
	PurplePluginPrefFrame *frame;
	PurplePluginPref *ppref;

	frame = purple_plugin_pref_frame_new();

	ppref = purple_plugin_pref_new_with_label(_("Conversation Placement"));
	purple_plugin_pref_frame_add(frame, ppref);

	/* Translators: "New conversations" should match the text in the preferences dialog and "By conversation count" should be the same text used above */
	ppref = purple_plugin_pref_new_with_label(_("Note: The preference for \"New conversations\" must be set to \"By conversation count\"."));
	purple_plugin_pref_set_pref_type(ppref, PURPLE_PLUGIN_PREF_INFO);
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
							"/plugins/gtk/extplacement/placement_number",
							_("Number of conversations per window"));
	purple_plugin_pref_set_bounds(ppref, 1, 50);
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
							"/plugins/gtk/extplacement/placement_number_separate",
							_("Separate IM and Chat windows when placing by number"));
	purple_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static PidginPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Stu Tomlinson <stu@nosnilmot.com>",
		NULL
	};

	return pidgin_plugin_info_new(
		"id",             "gtk-extplacement",
		"name",           N_("ExtPlacement"),
		"version",        DISPLAY_VERSION,
		"category",       N_("User interface"),
		"summary",        N_("Extra conversation placement options."),
		"description",    N_("Restrict the number of conversations per "
		                     "windows, optionally separating IMs and "
		                     "Chats"),
		"authors",        authors,
		"website",        PURPLE_WEBSITE,
		"abi-version",    PURPLE_ABI_VERSION,
		"pref-frame-cb",  get_plugin_pref_frame,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	purple_prefs_add_none("/plugins/gtk/extplacement");
	purple_prefs_add_int("/plugins/gtk/extplacement/placement_number", 4);
	purple_prefs_add_bool("/plugins/gtk/extplacement/placement_number_separate", FALSE);

	pidgin_conv_placement_add_fnc("number", _("By conversation count"),
							   &conv_placement_by_number);
	purple_prefs_trigger_callback(PIDGIN_PREFS_ROOT "/conversations/placement");
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	pidgin_conv_placement_remove_fnc("number");
	purple_prefs_trigger_callback(PIDGIN_PREFS_ROOT "/conversations/placement");
	return TRUE;
}

PURPLE_PLUGIN_INIT(extplacement, plugin_query, plugin_load, plugin_unload);
