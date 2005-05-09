/**
 * @file state.c State functions and definitions
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
#include "msn.h"
#include "state.h"

static const char *away_text[] =
{
	N_("Available"),
	N_("Available"),
	N_("Busy"),
	N_("Idle"),
	N_("Be Right Back"),
	N_("Away From Computer"),
	N_("On The Phone"),
	N_("Out To Lunch"),
	N_("Available"),
	N_("Available")
};

void
msn_change_status(MsnSession *session, MsnAwayType state)
{
	MsnCmdProc *cmdproc;
	MsnUser *user;
	MsnObject *msnobj;
	const char *state_text;

	g_return_if_fail(session != NULL);
	g_return_if_fail(session->notification != NULL);

	cmdproc = session->notification->cmdproc;
	user = session->user;
	state_text = msn_state_get_text(state);
	session->state = state;

	/* If we're not logged in yet, don't send the status to the server,
	 * it will be sent when login completes
	 */
	if (!session->logged_in)
		return;

	msnobj = msn_user_get_object(user);

	if (msnobj == NULL)
	{
		msn_cmdproc_send(cmdproc, "CHG", "%s %d", state_text,
						 MSN_CLIENT_ID);
	}
	else
	{
		char *msnobj_str;

		msnobj_str = msn_object_to_string(msnobj);

		msn_cmdproc_send(cmdproc, "CHG", "%s %d %s", state_text,
						 MSN_CLIENT_ID, gaim_url_encode(msnobj_str));

		g_free(msnobj_str);
	}
}

const char *
msn_away_get_text(MsnAwayType type)
{
	g_return_val_if_fail(type <= MSN_HIDDEN, NULL);

	return _(away_text[type]);
}

const char *
msn_state_get_text(MsnAwayType state)
{
	static char *status_text[] =
	{ "NLN", "NLN", "BSY", "IDL", "BRB", "AWY", "PHN", "LUN", "HDN", "HDN" };

	return status_text[state];
}
