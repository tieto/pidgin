/*
 * Gaim's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "oscar.h"

/**
 * Allocates a new OscarSession and initializes it with default values.
 */
OscarSession *
oscar_session_new(void)
{
	OscarSession *sess;

	sess = g_new0(OscarSession, 1);

	sess->queue_outgoing = NULL;
	sess->queue_incoming = NULL;
	aim_initsnachash(sess);
	sess->msgcookies = NULL;
	sess->modlistv = NULL;
	sess->snacid_next = 0x00000001;

	sess->locate.userinfo = NULL;
	sess->locate.torequest = NULL;
	sess->locate.requested = NULL;
	sess->locate.waiting_for_response = FALSE;
	sess->ssi.received_data = 0;
	sess->ssi.numitems = 0;
	sess->ssi.official = NULL;
	sess->ssi.local = NULL;
	sess->ssi.pending = NULL;
	sess->ssi.timestamp = (time_t)0;
	sess->ssi.waiting_for_ack = 0;
	sess->icq_info = NULL;
	sess->authinfo = NULL;
	sess->emailinfo = NULL;
	sess->peer_connections = NULL;

	/*
	 * This must always be set.  Default to the queue-based
	 * version for back-compatibility.
	 */
	aim_tx_setenqueue(sess, AIM_TX_QUEUED, NULL);

	/*
	 * Register all the modules for this session...
	 */
	aim__registermodule(sess, misc_modfirst); /* load the catch-all first */
	aim__registermodule(sess, service_modfirst);
	aim__registermodule(sess, locate_modfirst);
	aim__registermodule(sess, buddylist_modfirst);
	aim__registermodule(sess, msg_modfirst);
	aim__registermodule(sess, adverts_modfirst);
	aim__registermodule(sess, invite_modfirst);
	aim__registermodule(sess, admin_modfirst);
	aim__registermodule(sess, popups_modfirst);
	aim__registermodule(sess, bos_modfirst);
	aim__registermodule(sess, search_modfirst);
	aim__registermodule(sess, stats_modfirst);
	aim__registermodule(sess, translate_modfirst);
	aim__registermodule(sess, chatnav_modfirst);
	aim__registermodule(sess, chat_modfirst);
	aim__registermodule(sess, odir_modfirst);
	aim__registermodule(sess, bart_modfirst);
	/* missing 0x11 - 0x12 */
	aim__registermodule(sess, ssi_modfirst);
	/* missing 0x14 */
	aim__registermodule(sess, icq_modfirst);
	/* missing 0x16 */
	aim__registermodule(sess, auth_modfirst);
	aim__registermodule(sess, email_modfirst);

	return sess;
}

/**
 * Logoff and deallocate a session.
 *
 * @param sess Session to kill
 */
void
oscar_session_destroy(OscarSession *sess)
{
	aim_cleansnacs(sess, -1);

	while (sess->oscar_connections != NULL)
		oscar_connection_destroy(sess, sess->oscar_connections->data);

	aim__shutdownmodules(sess);

	g_free(sess);
}
