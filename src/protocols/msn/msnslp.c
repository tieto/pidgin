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
#include "msn.h"
#include "msnslp.h"

static void
send_ack(MsnSlpSession *slpsession, MsnMessage *acked_msg)
{
	MsnMessage *msg;

	msg = msn_message_new();

	msg->msnslp_message = TRUE;

	msg->msnslp_header.length = acked_msg->msnslp_header.length;
	msg->msnslp_header.flags = 0x02;
	msg->msnslp_header.ack_session_id =
		acked_msg->msnslp_header.ack_session_id;
	msg->msnslp_header.ack_unique_id =
		acked_msg->msnslp_header.id;
	msg->msnslp_header.ack_length = acked_msg->msnslp_header.length;

	/* Send this puppy. */
	msn_slp_session_send_msg(slpsession, msg);
}

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

gboolean
msn_slp_session_msg_received(MsnSlpSession *slpsession, MsnMessage *msg)
{
	const char *body;

	g_return_val_if_fail(slpsession != NULL,  TRUE);
	g_return_val_if_fail(msg != NULL,         TRUE);
	g_return_val_if_fail(msg->msnslp_message, TRUE);
	g_return_val_if_fail(!strcmp(msn_message_get_content_type(msg),
								 "application/x-msnmsgrp2p"), TRUE);

	body = msn_message_get_body(msg);

	if (strlen(body) == 0)
	{
		/* ACK. Ignore it. */
		return FALSE;
	}

	/* Now send an ack, since we got this. */
	send_ack(slpsession, msg);

	return FALSE;
}

void
msn_slp_session_send_msg(MsnSlpSession *slpsession, MsnMessage *msg)
{
	g_return_if_fail(slpsession != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(msg->msnslp_message);

	msg->msnslp_header.session_id = slpsession->session_id;

	if (slpsession->base_id == 0)
	{
		slpsession->base_id = rand() % 0xFFFFFF00 + 4;
		slpsession->prev_msg_id = slpsession->base_id;
	}
	else
		slpsession->prev_msg_id = ++slpsession->base_id;

	msg->msnslp_header.id = slpsession->prev_msg_id;
	msg->msnslp_header.ack_session_id = rand() % 0xFFFFFF00;
	msg->msnslp_header.total_size = strlen(msn_message_get_body(msg));

	msn_message_set_charset(msg, NULL);

	msn_message_set_flag(msg, 'D');
	msn_message_set_content_type(msg, "application/x-msnmsgrp2p");
	msn_message_set_attr(msg, "P2P-Dest",
			msn_user_get_passport(msn_message_get_receiver(msg)));

	msn_switchboard_send_msg(slpsession->swboard, msg);
}

void
msn_slp_session_request_user_display(MsnSlpSession *slpsession,
									 const MsnUser *localUser,
									 const MsnUser *remoteUser,
									 const MsnObject *obj)
{
	MsnMessage *invite_msg;
	char *msnobj_data;
	char *msnobj_base64;
	char *content;
	char *header;

	g_return_if_fail(slpsession != NULL);
	g_return_if_fail(localUser  != NULL);
	g_return_if_fail(remoteUser != NULL);
	g_return_if_fail(obj        != NULL);

	msnobj_data = msn_object_to_string(obj);
	msnobj_base64 = tobase64(msnobj_data, strlen(msnobj_data));
	g_free(msnobj_data);

	if (slpsession->session_id == 0)
		slpsession->session_id = rand() % 0xFFFFFF00;

	content = g_strdup_printf(
		"EUF-GUID: {A4268EEC-FEC5-49E5-95C3-F126696BDBF6}\r\n"
		"SessionID: %ld\r\n"
		"AppID: 1\r\n"
		"Context: %s",
		slpsession->session_id,
		msnobj_base64);

	g_free(msnobj_base64);

	header = g_strdup_printf(
		"INVITE MSNMSGR:%s MSNSLP/1.0\r\n"
		"To: <msnmsgr:%s>\r\n"
		"From: <msnmsgr:%s>\r\n"
		"Via: MSNSLP/1.0/TLP ;branch={33517CE4-02FC-4428-B6F4-39927229B722}\r\n"
		"CSeq: 0\r\n"
		"Call-ID: {9D79AE57-1BD5-444B-B14E-3FC9BB2B5D58}\r\n"
		"Max-Forwards: 0\r\n"
		"Content-Type: application/x-msnmsgr-sessionreqbody\r\n"
		"Content-Length: %d\r\n"
		"\r\n"
		"%s"
		"\r\n\r\n",
		msn_user_get_passport(remoteUser),
		msn_user_get_passport(localUser),
		msn_user_get_passport(remoteUser),
		strlen(content) + 5,
		content);

	invite_msg = msn_message_new();

	msn_slp_session_send_msg(slpsession, invite_msg);

	msn_message_destroy(invite_msg);
}

gboolean
msn_p2p_msg(MsnServConn *servconn, MsnMessage *msg)
{
	MsnSwitchBoard *swboard = servconn->data;
	gboolean session_ended = FALSE;

	if (swboard->slp_session == NULL)
		swboard->slp_session = msn_slp_session_new(swboard, TRUE);

	session_ended = msn_slp_session_msg_received(swboard->slp_session, msg);

	if (session_ended)
		msn_slp_session_destroy(swboard->slp_session);

	return FALSE;
}
