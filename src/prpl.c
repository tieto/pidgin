/*
 * gaim
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
 *
 */
#include "internal.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "request.h"
#include "util.h"

GaimPlugin *
gaim_find_prpl(const char *id)
{
	GList *l;
	GaimPlugin *plugin;

	for (l = gaim_plugins_get_protocols(); l != NULL; l = l->next) {
		plugin = (GaimPlugin *)l->data;

		/* Just In Case (TM) */
		if (GAIM_IS_PROTOCOL_PLUGIN(plugin)) {

			if (!strcmp(plugin->info->id, id))
				return plugin;
		}
	}

	return NULL;
}
