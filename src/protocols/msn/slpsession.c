/**
 * @file slpsession.h SLP Session functions
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
#include "slpsession.h"

/**************************************************************************
 * SLP Session
 **************************************************************************/

MsnSlpSession *
msn_slp_session_new(MsnSlpCall *slpcall)
{
	MsnSlpSession *slpsession;

	g_return_val_if_fail(slpcall != NULL, NULL);

	slpsession = g_new0(MsnSlpSession, 1);

	slpsession->slpcall = slpcall;
	slpsession->id = slpcall->session_id;
	slpsession->call_id = slpcall->id;
	slpsession->app_id = slpcall->app_id;

	slpcall->slplink->slp_sessions =
		g_list_append(slpcall->slplink->slp_sessions, slpsession);

	return slpsession;
}

void
msn_slp_session_destroy(MsnSlpSession *slpsession)
{
	g_return_if_fail(slpsession != NULL);

	if (slpsession->call_id != NULL)
		g_free(slpsession->call_id);

	slpsession->slpcall->slplink->slp_sessions =
		g_list_remove(slpsession->slpcall->slplink->slp_sessions, slpsession);

	g_free(slpsession);
}

#if 0
static void
msn_slp_session_send_slpmsg(MsnSlpSession *slpsession, MsnSlpMessage *slpmsg)
{
	slpmsg->slpsession = slpsession;

#if 0
	slpmsg->session_id = slpsession->id;
	slpmsg->app_id = slpsession->app_id;
#endif

	msn_slplink_send_slpmsg(slpsession->slpcall->slplink, slpmsg);
}
#endif
