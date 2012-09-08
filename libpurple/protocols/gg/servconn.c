/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#include "servconn.h"

#include "utils.h"

#include <debug.h>
#include <glibcompat.h>

#define GGP_SERVCONN_HISTORY_PREF "/plugins/prpl/gg/server_history"
#define GGP_SERVCONN_HISTORY_MAXLEN 15

typedef struct
{
	GList *server_history;
	PurpleAccountOption *server_option;
} ggp_servconn_global_data;

static ggp_servconn_global_data global_data;

void ggp_servconn_setup(PurpleAccountOption *server_option)
{
	purple_prefs_add_string(GGP_SERVCONN_HISTORY_PREF, "");
	
	global_data.server_option = server_option;
	global_data.server_history = ggp_strsplit_list(purple_prefs_get_string(
		GGP_SERVCONN_HISTORY_PREF), ";", GGP_SERVCONN_HISTORY_MAXLEN + 1);
	global_data.server_history = ggp_list_truncate(
		global_data.server_history, GGP_SERVCONN_HISTORY_MAXLEN,
		g_free);
	
	purple_account_option_string_set_hints(global_data.server_option,
		ggp_servconn_get_servers());
}

void ggp_servconn_cleanup(void)
{
	g_list_free_full(global_data.server_history, &g_free);
}

void ggp_servconn_add_server(const gchar *server)
{
	GList *old_entry;
	
	old_entry = g_list_find_custom(global_data.server_history, server,
		(GCompareFunc)g_strcmp0);
	if (old_entry)
	{
		g_free(old_entry->data);
		global_data.server_history = g_list_delete_link(
			global_data.server_history, old_entry);
	}
	
	global_data.server_history = g_list_prepend(global_data.server_history,
		g_strdup(server));
	global_data.server_history = ggp_list_truncate(
		global_data.server_history, GGP_SERVCONN_HISTORY_MAXLEN,
		g_free);

	purple_prefs_set_string(GGP_SERVCONN_HISTORY_PREF, ggp_strjoin_list(";",
		global_data.server_history));
	purple_account_option_string_set_hints(global_data.server_option,
		ggp_servconn_get_servers());
}

GSList * ggp_servconn_get_servers(void)
{
	return ggp_list_copy_to_slist_deep(global_data.server_history,
		(GCopyFunc)g_strdup, NULL);
}
