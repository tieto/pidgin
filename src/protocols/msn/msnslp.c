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

	if (session->orig_body != NULL)
		g_free(session->orig_body);

	if (session->outgoing_msg != NULL)
		msn_message_unref(session->outgoing_msg);

	if (session->call_id != NULL)
		g_free(session->call_id);

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
		gaim_debug_info("msn", "Received MSNSLP ACK\n");

		return FALSE;
	}

	/* Now send an ack, since we got this. */
	msn_slp_session_send_ack(slpsession, msg);

	return FALSE;
}

static void
send_msg_part(MsnSlpSession *slpsession, MsnMessage *msg)
{
	msg->msnslp_header.length =
		(slpsession->orig_len - slpsession->offset > 1202
		 ? 1202 : slpsession->orig_len - slpsession->offset);

	if (slpsession->offset > 0)
	{
		if (msg->bin_content)
		{
			msn_message_set_bin_data(msg,
				slpsession->orig_body + slpsession->offset,
				msg->msnslp_header.length);
		}
		else
		{
			msn_message_set_body(msg,
				slpsession->orig_body + slpsession->offset);
		}
	}

	msg->msnslp_header.offset_1 = slpsession->offset;

	msn_switchboard_send_msg(slpsession->swboard, msg);

	if (slpsession->offset + msg->msnslp_header.length == slpsession->orig_len)
	{
		msn_message_destroy(msg);

		g_free(slpsession->orig_body);

		slpsession->offset       = 0;
		slpsession->orig_len     = 0;
		slpsession->orig_body    = NULL;
		slpsession->outgoing_msg = NULL;
	}
	else
		slpsession->offset += msg->msnslp_header.length;
}

void
msn_slp_session_send_msg(MsnSlpSession *slpsession, MsnMessage *msg)
{
	g_return_if_fail(slpsession != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(msg->msnslp_message);
	g_return_if_fail(slpsession->outgoing_msg == NULL);

	msg->msnslp_header.session_id = slpsession->session_id;

	slpsession->outgoing_msg = msn_message_ref(msg);

	if (slpsession->base_id == 0)
	{
		slpsession->base_id = rand() % 0x0FFFFFF0 + 4;
		slpsession->prev_msg_id = slpsession->base_id;
	}
	else if (slpsession->offset == 0)
		slpsession->prev_msg_id = ++slpsession->base_id;

	msg->msnslp_header.id = slpsession->prev_msg_id;
	/*msg->msnslp_header.ack_session_id = rand() % 0xFFFFFF00;*/
	msg->msnslp_header.ack_session_id = 0x1407C7DE;

	msn_message_set_charset(msg, NULL);

	if (msg->msnslp_header.session_id != 0)
		msg->msnslp_footer.app_id = 1;

	if (msg->bin_content)
	{
		const void *temp;

		temp = msn_message_get_bin_data(msg, &slpsession->orig_len);

		slpsession->orig_body = g_memdup(temp, slpsession->orig_len);
	}
	else
	{
		slpsession->orig_body = g_strdup(msn_message_get_body(msg));
		slpsession->orig_len  = strlen(slpsession->orig_body);
	}

	msg->msnslp_header.total_size_1 = slpsession->orig_len;

	send_msg_part(slpsession, msg);
}

void
msn_slp_session_send_ack(MsnSlpSession *slpsession, MsnMessage *acked_msg)
{
	MsnMessage *msg;
	gboolean new_base_id = FALSE;

	g_return_if_fail(slpsession != NULL);
	g_return_if_fail(acked_msg != NULL);
	g_return_if_fail(acked_msg->msnslp_message);
	g_return_if_fail(slpsession->outgoing_msg == NULL);

	msg = msn_message_new_msnslp_ack(acked_msg);

	if (slpsession->base_id == 0)
	{
		slpsession->base_id = rand() % 0x0FFFFE00 + 10;
		slpsession->prev_msg_id = slpsession->base_id;

		new_base_id = TRUE;
	}
	else
		slpsession->prev_msg_id = ++slpsession->base_id;

	msg->msnslp_header.id = slpsession->prev_msg_id;

	if (new_base_id)
		slpsession->prev_msg_id -= 4;

	msn_switchboard_send_msg(slpsession->swboard, msg);
}

void
msn_slp_session_request_user_display(MsnSlpSession *slpsession,
									 MsnUser *local_user,
									 MsnUser *remote_user,
									 const MsnObject *obj)
{
	MsnMessage *invite_msg;
	long session_id;
	char *msnobj_data;
	char *msnobj_base64;
	char *branch;
	char *content;
	char *body;

	g_return_if_fail(slpsession  != NULL);
	g_return_if_fail(local_user  != NULL);
	g_return_if_fail(remote_user != NULL);
	g_return_if_fail(obj         != NULL);

	msnobj_data = msn_object_to_string(obj);
	msnobj_base64 = gaim_base64_encode(msnobj_data, strlen(msnobj_data));
	g_free(msnobj_data);

#if 0
	if ((c = strchr(msnobj_base64, '=')) != NULL)
		*c = '\0';
#endif

	session_id = rand() % 0xFFFFFF00 + 4;

	branch = g_strdup_printf("%4X%4X-%4X-%4X-%4X-%4X%4X%4X",
							 rand() % 0xAAFF + 0x1111,
							 rand() % 0xAAFF + 0x1111,
							 rand() % 0xAAFF + 0x1111,
							 rand() % 0xAAFF + 0x1111,
							 rand() % 0xAAFF + 0x1111,
							 rand() % 0xAAFF + 0x1111,
							 rand() % 0xAAFF + 0x1111,
							 rand() % 0xAAFF + 0x1111);

	slpsession->call_id = g_strdup_printf("%4X%4X-%4X-%4X-%4X-%4X%4X%4X",
										  rand() % 0xAAFF + 0x1111,
										  rand() % 0xAAFF + 0x1111,
										  rand() % 0xAAFF + 0x1111,
										  rand() % 0xAAFF + 0x1111,
										  rand() % 0xAAFF + 0x1111,
										  rand() % 0xAAFF + 0x1111,
										  rand() % 0xAAFF + 0x1111,
										  rand() % 0xAAFF + 0x1111);

	content = g_strdup_printf(
		"EUF-GUID: {A4268EEC-FEC5-49E5-95C3-F126696BDBF6}\r\n"
		"SessionID: %lu\r\n"
		"AppID: 1\r\n"
		"Context: %s",
		session_id,
		msnobj_base64);

	g_free(msnobj_base64);

	body = g_strdup_printf(
		"INVITE MSNMSGR:%s MSNSLP/1.0\r\n"
		"To: <msnmsgr:%s>\r\n"
		"From: <msnmsgr:%s>\r\n"
		"Via: MSNSLP/1.0/TLP ;branch={%s}\r\n"
		"CSeq: 0\r\n"
		"Call-ID: {%s}\r\n"
		"Max-Forwards: 0\r\n"
		"Content-Type: application/x-msnmsgr-sessionreqbody\r\n"
		"Content-Length: %d\r\n"
		"\r\n"
		"%s"
		"\r\n\r\n",
		msn_user_get_passport(remote_user),
		msn_user_get_passport(remote_user),
		msn_user_get_passport(local_user),
		branch,
		slpsession->call_id,
		(int)strlen(content) + 5,
		content);

	g_free(content);
	g_free(branch);

	gaim_debug_misc("msn", "Message = {%s}\n", body);

	invite_msg = msn_message_new_msnslp();

	msn_message_set_sender(invite_msg, local_user);
	msn_message_set_receiver(invite_msg, remote_user);

	msn_message_set_body(invite_msg, body);

	g_free(body);

	msn_slp_session_send_msg(slpsession, invite_msg);

	msn_message_destroy(invite_msg);
}

gboolean
msn_p2p_msg(MsnServConn *servconn, MsnMessage *msg)
{
	MsnSwitchBoard *swboard = servconn->data;
	gboolean session_ended = FALSE;

	if (swboard->slp_session == NULL)
		swboard->slp_session = msn_slp_session_new(swboard, FALSE);

	session_ended = msn_slp_session_msg_received(swboard->slp_session, msg);

	if (session_ended)
		msn_slp_session_destroy(swboard->slp_session);

	return FALSE;
}
