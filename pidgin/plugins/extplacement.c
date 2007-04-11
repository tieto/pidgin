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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
		win = pidgin_conv_window_last_with_type(purple_conversation_get_type(conv->active_conv));
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
					purple_conversation_get_type(pidgin_conv_window_get_active_conversation(win)) != purple_conversation_get_type(conv->active_conv))
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

static gboolean
plugin_load(PurplePlugin *plugin)
{
	pidgin_conv_placement_add_fnc("number", _("By conversation count"),
							   &conv_placement_by_number);
	purple_prefs_trigger_callback(PIDGIN_PREFS_ROOT "/conversations/placement");
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	pidgin_conv_placement_remove_fnc("number");
	purple_prefs_trigger_callback(PIDGIN_PREFS_ROOT "/conversations/placement");
	return TRUE;
}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin) {
	PurplePluginPrefFrame *frame;
	PurplePluginPref *ppref;

	frame = purple_plugin_pref_frame_new();

	ppref = purple_plugin_pref_new_with_label(_("Conversation Placement"));
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

static PurplePluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,   /* page_num (Reserved) */
	NULL /* frame (Reserved) */
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,							/**< type			*/
	PIDGIN_PLUGIN_TYPE,							/**< ui_requirement	*/
	0,												/**< flags			*/
	NULL,											/**< dependencies	*/
	PURPLE_PRIORITY_DEFAULT,							/**< priority		*/
	"gtk-extplacement",								/**< id				*/
	N_("ExtPlacement"),								/**< name			*/
	VERSION,										/**< version		*/
	N_("Extra conversation placement options."),	/**< summary		*/
													/**  description	*/
	N_("Restrict the number of conversations per windows,"
	   " optionally separating IMs and Chats"),
	"Stu Tomlinson <stu@nosnilmot.com>",			/**< author			*/
	PURPLE_WEBSITE,									/**< homepage		*/
	plugin_load,									/**< load			*/
	plugin_unload,									/**< unload			*/
	NULL,											/**< destroy		*/
	NULL,											/**< ui_info		*/
	NULL,											/**< extra_info		*/
	&prefs_info,									/**< prefs_info		*/
	NULL											/**< actions		*/
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none("/plugins/gtk/extplacement");
	purple_prefs_add_int("/plugins/gtk/extplacement/placement_number", 4);
	purple_prefs_add_bool("/plugins/gtk/extplacement/placement_number_separate", FALSE);
}

PURPLE_INIT_PLUGIN(extplacement, init_plugin, info)
