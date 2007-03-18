/**
 * @file gntconn.c GNT Connection API
 * @ingroup gntui
 *
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
 */
#include "account.h"
#include "core.h"
#include "request.h"

#include "gntconn.h"
#include "gntgaim.h"

static void
finch_connection_report_disconnect(GaimConnection *gc, const char *text)
{
	char *act, *primary, *secondary;
	GaimAccount *account = gaim_connection_get_account(gc);

	act = g_strdup_printf(_("%s (%s)"), gaim_account_get_username(account),
			gaim_account_get_protocol_name(account));

	primary = g_strdup_printf(_("%s disconnected."), act);
	secondary = g_strdup_printf(_("%s was disconnected due to the following error:\n%s"),
			act, text);

	gaim_request_action(account, _("Connection Error"), primary, secondary, 1,
						account, 2,
						_("OK"), NULL,
						_("Connect"),
						GAIM_CALLBACK(gaim_account_connect));

	g_free(act);
	g_free(primary);
	g_free(secondary);
}

static GaimConnectionUiOps ops = 
{
	.connect_progress = NULL,
	.connected = NULL,
	.disconnected = NULL,
	.notice = NULL,
	.report_disconnect = finch_connection_report_disconnect
};

GaimConnectionUiOps *finch_connections_get_ui_ops()
{
	return &ops;
}

void finch_connections_init()
{}

void finch_connections_uninit()
{}

