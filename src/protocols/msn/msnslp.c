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

	if (session->branch != NULL)
		g_free(session->branch);

	g_free(session);
}

static void
msn_slp_session_send_message(MsnSlpSession *slpsession,
							 MsnMessage *source_msg,
							 MsnUser *local_user, MsnUser *remote_user,
							 const char *header, const char *branch,
							 int cseq, const char *call_id,
							 const char *content)
{
	MsnMessage *invite_msg;
	char *body;

	g_return_if_fail(slpsession != NULL);
	g_return_if_fail(header     != NULL);
	g_return_if_fail(branch     != NULL);
	g_return_if_fail(call_id    != NULL);

	if (source_msg != NULL)
	{
		if (msn_message_is_incoming(source_msg))
			remote_user = msn_message_get_sender(source_msg);
		else
			remote_user = msn_message_get_receiver(source_msg);

		local_user = slpsession->swboard->servconn->session->user;
	}

	if (branch == NULL)
		branch = "null";

	body = g_strdup_printf(
		"%s\r\n"
		"To: <msnmsgr:%s>\r\n"
		"From: <msnmsgr:%s>\r\n"
		"Via: MSNSLP/1.0/TLP ;branch={%s}\r\n"
		"CSeq: %d\r\n"
		"Call-ID: {%s}\r\n"
		"Max-Forwards: 0\r\n"
		"Content-Type: application/x-msnmsgr-sessionreqbody\r\n"
		"Content-Length: %d\r\n"
		"\r\n"
		"%s"
		"\r\n\r\n",
		header,
		msn_user_get_passport(remote_user),
		msn_user_get_passport(local_user),
		branch, cseq, call_id,
		(content == NULL ? 0  : (int)strlen(content) + 5),
		(content == NULL ? "" : content));

	gaim_debug_misc("msn", "Message = {%s}\n", body);

	invite_msg = msn_message_new_msnslp();

	msn_message_set_sender(invite_msg, local_user);
	msn_message_set_receiver(invite_msg, remote_user);

	msn_message_set_body(invite_msg, body);

	g_free(body);

	msn_slp_session_send_msg(slpsession, invite_msg);
}

static gboolean
send_error_500(MsnSlpSession *slpsession, const char *call_id, MsnMessage *msg)
{
	g_return_val_if_fail(slpsession != NULL, TRUE);
	g_return_val_if_fail(msg        != NULL, TRUE);

	msn_slp_session_send_message(slpsession, msg, NULL, NULL,
								 "MSNSLP/1.0 500 Internal Error",
								 slpsession->branch, 1, call_id, NULL);

	return TRUE;
}

static gboolean
send_cb(gpointer user_data)
{
	MsnSlpSession *slpsession = (MsnSlpSession *)user_data;
	MsnMessage *msg;
	char data[1200];
	size_t len;

	len = fread(data, 1, 1200, slpsession->send_fp);

	slpsession->remaining_size -= len;

	msg = msn_message_new_msnslp();
	msn_message_set_sender(msg,   slpsession->receiver);
	msn_message_set_receiver(msg, slpsession->sender);
	msn_message_set_bin_data(msg, data, len);

	msn_slp_session_send_msg(slpsession, msg);

	if (slpsession->remaining_size <= 0)
	{
		slpsession->send_timer = 0;

		return FALSE;
	}

	return TRUE;
}

gboolean
msn_slp_session_msg_received(MsnSlpSession *slpsession, MsnMessage *msg)
{
	const char *body;
	const char *c, *c2;
	GaimAccount *account;

	g_return_val_if_fail(slpsession != NULL,  TRUE);
	g_return_val_if_fail(msg != NULL,         TRUE);
	g_return_val_if_fail(msg->msnslp_message, TRUE);
	g_return_val_if_fail(!strcmp(msn_message_get_content_type(msg),
								 "application/x-msnmsgrp2p"), TRUE);

	account = slpsession->swboard->servconn->session->account;

	body = msn_message_get_body(msg);

	gaim_debug_misc("msn", "MSNSLP Message: {%s}\n", body);

	if (*body == '\0')
	{
		/* ACK. Ignore it. */
		gaim_debug_info("msn", "Received MSNSLP ACK\n");

		return FALSE;
	}

	if (slpsession->send_fp != NULL && slpsession->remaining_size == 0)
	{
		/*
		 * In theory, if we received something while send_fp is non-NULL,
		 * but remaining_size is 0, then this is a data ack message.
		 *
		 * Say BYE-BYE.
		 */
		char *header;

		fclose(slpsession->send_fp);
		slpsession->send_fp = NULL;

		header = g_strdup_printf("BYE MSNMSGR:%s MSNSLP/1.0",
			msn_user_get_passport(msn_message_get_sender(msg)));

		msn_slp_session_send_message(slpsession, msg, NULL, NULL, header,
									 "A0D624A6-6C0C-4283-A9E0-BC97B4B46D32",
									 0, slpsession->call_id, "");

		g_free(header);

		return TRUE;
	}

	if (!strncmp(body, "MSNSLP/1.0 ", strlen("MSNSLP/1.0 ")))
	{
		/* Make sure this is "OK" */
		const char *status = body + strlen("MSNSLP/1.0 ");

		if (strncmp(status, "200 OK", 6))
		{
			/* It's not valid. Kill this off. */
			char temp[32];
			const char *c;

			/* Eww */
			if ((c = strchr(status, '\r')) || (c = strchr(status, '\n')) ||
				(c = strchr(status, '\0')))
			{
				strncpy(temp, status, c - status);
				temp[c - status] = '\0';
			}

			gaim_debug_error("msn", "Received non-OK result: %s\n", temp);

			return TRUE;
		}
	}
	else if (!strncmp(body, "INVITE", strlen("INVITE")))
	{
		/* Parse it. Oh this is fun. */
		char *branch, *call_id, *temp;
		unsigned int session_id, app_id;

		/* First, branch. */
		if ((c = strstr(body, ";branch={")) == NULL)
			return send_error_500(slpsession, NULL, msg);

		c += strlen(";branch={");

		if ((c2 = strchr(c, '}')) == NULL)
			return send_error_500(slpsession, NULL, msg);

		branch = g_strndup(c, c2 - c);

		if (slpsession->branch != NULL)
			slpsession->branch = branch;

		/* Second, Call-ID */
		if ((c = strstr(body, "Call-ID: {")) == NULL)
			return send_error_500(slpsession, NULL, msg);

		c += strlen("Call-ID: {");

		if ((c2 = strchr(c, '}')) == NULL)
			return send_error_500(slpsession, NULL, msg);

		call_id = g_strndup(c, c2 - c);

		if (slpsession->call_id != NULL)
			slpsession->call_id = call_id;

		/* Third, SessionID */
		if ((c = strstr(body, "SessionID: ")) == NULL)
			return send_error_500(slpsession, NULL, msg);

		c += strlen("SessionID: ");

		if ((c2 = strchr(c, '\r')) == NULL)
			return send_error_500(slpsession, NULL, msg);

		temp = g_strndup(c, c2 - c);
		session_id = atoi(temp);
		g_free(temp);

		/* Fourth, AppID */
		if ((c = strstr(body, "AppID: ")) == NULL)
			return send_error_500(slpsession, NULL, msg);

		c += strlen("AppID: ");

		if ((c2 = strchr(c, '\r')) == NULL)
			return send_error_500(slpsession, NULL, msg);

		temp = g_strndup(c, c2 - c);
		app_id = atoi(temp);
		g_free(temp);

		if (app_id == 1)
		{
			MsnMessage *new_msg;
			char *content;
			char nil_body[4];
			struct stat st;

			/* Send the 200 OK message. */
			content = g_strdup_printf("SessionID: %d", session_id);
			msn_slp_session_send_ack(slpsession, msg);

			msn_slp_session_send_message(slpsession, msg, NULL, NULL,
										 "MSNSLP/1.0 200 OK",
										 branch, 1, call_id, content);

			g_free(content);

			/* Send the Data Preparation message. */
			memset(nil_body, 0, sizeof(nil_body));

			slpsession->session_id = session_id;
			slpsession->receiver = msn_message_get_sender(msg);
			slpsession->sender = slpsession->swboard->servconn->session->user;

			new_msg = msn_message_new_msnslp();
			msn_message_set_sender(new_msg, slpsession->sender);
			msn_message_set_receiver(new_msg, slpsession->receiver);
			msn_message_set_bin_data(new_msg, nil_body, 4);
			new_msg->msnslp_footer.app_id = 1;

			msn_slp_session_send_msg(slpsession, new_msg);

			slpsession->send_fp =
				fopen(gaim_account_get_buddy_icon(account), "rb");

			if (stat(gaim_account_get_buddy_icon(account), &st) == 0)
				slpsession->remaining_size = st.st_size;

			slpsession->send_timer = gaim_timeout_add(10, send_cb, slpsession);
		}
		else
			return send_error_500(slpsession, call_id, msg);

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
	long session_id;
	char *msnobj_data;
	char *msnobj_base64;
	char *header;
	char *content;

	g_return_if_fail(slpsession  != NULL);
	g_return_if_fail(local_user  != NULL);
	g_return_if_fail(remote_user != NULL);
	g_return_if_fail(obj         != NULL);

	msnobj_data = msn_object_to_string(obj);
	msnobj_base64 = gaim_base64_encode(msnobj_data, strlen(msnobj_data));
	g_free(msnobj_data);

	session_id = rand() % 0xFFFFFF00 + 4;

	if (slpsession->branch != NULL)
		g_free(slpsession->branch);

	slpsession->branch = g_strdup_printf("%4X%4X-%4X-%4X-%4X-%4X%4X%4X",
										 rand() % 0xAAFF + 0x1111,
										 rand() % 0xAAFF + 0x1111,
										 rand() % 0xAAFF + 0x1111,
										 rand() % 0xAAFF + 0x1111,
										 rand() % 0xAAFF + 0x1111,
										 rand() % 0xAAFF + 0x1111,
										 rand() % 0xAAFF + 0x1111,
										 rand() % 0xAAFF + 0x1111);

	if (slpsession->call_id != NULL)
		g_free(slpsession->call_id);

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

	header = g_strdup_printf("INVITE MSNMSGR:%s MSNSLP/1.0",
							 msn_user_get_passport(remote_user));

	msn_slp_session_send_message(slpsession, NULL, local_user, remote_user,
								 header, slpsession->branch, 0,
								 slpsession->call_id, content);

	g_free(header);
	g_free(content);
}

gboolean
msn_p2p_msg(MsnServConn *servconn, MsnMessage *msg)
{
	MsnSwitchBoard *swboard = servconn->data;
	gboolean session_ended = FALSE;

#if 0
	FILE *fp;
	size_t len;
	char *buf;

	buf = msn_message_to_string(msg, &len);
	/* Windows doesn't like Unix paths */
	fp = fopen("/tmp/msn-msg", "ab");
	fwrite(buf, 1, len, fp);
	fclose(fp);

	g_free(buf);
#endif

	if (swboard->slp_session == NULL)
		swboard->slp_session = msn_slp_session_new(swboard, FALSE);

	session_ended = msn_slp_session_msg_received(swboard->slp_session, msg);

	if (session_ended)
		msn_slp_session_destroy(swboard->slp_session);

	return FALSE;
}
