/**
 * @file msnslp.c MSNSLP support
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "msnslp.h"

MsnSlpSession *
msn_slp_session_new(MsnSwitchBoard *swboard, gboolean local_initiated)
{
	MsnSlpSession *slpsession;

	g_return_val_if_fail(swboard != NULL, NULL);

	slpsession = g_new0(MsnSlpSession, 1);

	slpsession->swboard = swboard;
	slpsession->local_initiated = local_initiated;

	return slpsession;
}

void
msn_slp_session_destroy(MsnSlpSession *session)
{
	g_return_if_fail(session != NULL);

	g_free(session);
}

void
msn_slp_session_send_msg(MsnSlpSession *session, MsnMessage *msg)
{
	g_return_if_fail(session != NULL);
	g_return_if_fail(msg != NULL);

}
