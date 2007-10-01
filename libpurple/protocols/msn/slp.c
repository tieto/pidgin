/**
 * @file msnslp.c MSNSLP support
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "msn.h"
#include "slp.h"
#include "slpcall.h"
#include "slpmsg.h"
#include "slpsession.h"

#include "object.h"
#include "user.h"
#include "switchboard.h"

/* ms to delay between sending buddy icon requests to the server. */
#define BUDDY_ICON_DELAY 20000

static void send_ok(MsnSlpCall *slpcall, const char *branch,
					const char *type, const char *content);

static void send_decline(MsnSlpCall *slpcall, const char *branch,
						 const char *type, const char *content);

void msn_request_user_display(MsnUser *user);

/**************************************************************************
 * Util
 **************************************************************************/

static char *
get_token(const char *str, const char *start, const char *end)
{
	const char *c, *c2;

	if ((c = strstr(str, start)) == NULL)
		return NULL;

	c += strlen(start);

	if (end != NULL)
	{
		if ((c2 = strstr(c, end)) == NULL)
			return NULL;

		return g_strndup(c, c2 - c);
	}
	else
	{
		/* This has to be changed */
		return g_strdup(c);
	}

}

/**************************************************************************
 * Xfer
 **************************************************************************/

static void
msn_xfer_init(PurpleXfer *xfer)
{
	MsnSlpCall *slpcall;
	/* MsnSlpLink *slplink; */
	char *content;

	purple_debug_info("msn", "xfer_init\n");

	slpcall = xfer->data;

	/* Send Ok */
	content = g_strdup_printf("SessionID: %lu\r\n\r\n",
							  slpcall->session_id);

	send_ok(slpcall, slpcall->branch, "application/x-msnmsgr-sessionreqbody",
			content);

	g_free(content);
	msn_slplink_unleash(slpcall->slplink);
}

void
msn_xfer_cancel(PurpleXfer *xfer)
{
	MsnSlpCall *slpcall;
	char *content;

	g_return_if_fail(xfer != NULL);
	g_return_if_fail(xfer->data != NULL);

	slpcall = xfer->data;

	if (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL)
	{
		if (slpcall->started)
		{
			msn_slp_call_close(slpcall);
		}
		else
		{
			content = g_strdup_printf("SessionID: %lu\r\n\r\n",
									slpcall->session_id);

			send_decline(slpcall, slpcall->branch, "application/x-msnmsgr-sessionreqbody",
						content);

			g_free(content);
			msn_slplink_unleash(slpcall->slplink);

			msn_slp_call_destroy(slpcall);
		}
	}
}

void
msn_xfer_progress_cb(MsnSlpCall *slpcall, gsize total_length, gsize len, gsize offset)
{
	PurpleXfer *xfer;

	xfer = slpcall->xfer;

	xfer->bytes_sent = (offset + len);
	xfer->bytes_remaining = total_length - (offset + len);

	purple_xfer_update_progress(xfer);
}

void
msn_xfer_end_cb(MsnSlpCall *slpcall, MsnSession *session)
{
	if ((purple_xfer_get_status(slpcall->xfer) != PURPLE_XFER_STATUS_DONE) &&
		(purple_xfer_get_status(slpcall->xfer) != PURPLE_XFER_STATUS_CANCEL_REMOTE) &&
		(purple_xfer_get_status(slpcall->xfer) != PURPLE_XFER_STATUS_CANCEL_LOCAL))
	{
		purple_xfer_cancel_remote(slpcall->xfer);
	}
}

void
msn_xfer_completed_cb(MsnSlpCall *slpcall, const guchar *body,
					  gsize size)
{
	PurpleXfer *xfer = slpcall->xfer;
	purple_xfer_set_completed(xfer, TRUE);
	purple_xfer_end(xfer);
}

/**************************************************************************
 * SLP Control
 **************************************************************************/

#if 0
static void
got_transresp(MsnSlpCall *slpcall, const char *nonce,
			  const char *ips_str, int port)
{
	MsnDirectConn *directconn;
	char **ip_addrs, **c;

	directconn = msn_directconn_new(slpcall->slplink);

	directconn->initial_call = slpcall;

	/* msn_directconn_parse_nonce(directconn, nonce); */
	directconn->nonce = g_strdup(nonce);

	ip_addrs = g_strsplit(ips_str, " ", -1);

	for (c = ip_addrs; *c != NULL; c++)
	{
		purple_debug_info("msn", "ip_addr = %s\n", *c);
		if (msn_directconn_connect(directconn, *c, port))
			break;
	}

	g_strfreev(ip_addrs);
}
#endif

static void
send_ok(MsnSlpCall *slpcall, const char *branch,
		const char *type, const char *content)
{
	MsnSlpLink *slplink;
	MsnSlpMessage *slpmsg;

	slplink = slpcall->slplink;

	/* 200 OK */
	slpmsg = msn_slpmsg_sip_new(slpcall, 1,
								"MSNSLP/1.0 200 OK",
								branch, type, content);

#ifdef MSN_DEBUG_SLP
	slpmsg->info = "SLP 200 OK";
	slpmsg->text_body = TRUE;
#endif

	msn_slplink_queue_slpmsg(slplink, slpmsg);

	msn_slp_call_session_init(slpcall);
}

static void
send_decline(MsnSlpCall *slpcall, const char *branch,
			 const char *type, const char *content)
{
	MsnSlpLink *slplink;
	MsnSlpMessage *slpmsg;

	slplink = slpcall->slplink;

	/* 603 Decline */
	slpmsg = msn_slpmsg_sip_new(slpcall, 1,
								"MSNSLP/1.0 603 Decline",
								branch, type, content);

#ifdef MSN_DEBUG_SLP
	slpmsg->info = "SLP 603 Decline";
	slpmsg->text_body = TRUE;
#endif

	msn_slplink_queue_slpmsg(slplink, slpmsg);
}

#define MAX_FILE_NAME_LEN 0x226

static void
got_sessionreq(MsnSlpCall *slpcall, const char *branch,
			   const char *euf_guid, const char *context)
{
	if (!strcmp(euf_guid, "A4268EEC-FEC5-49E5-95C3-F126696BDBF6"))
	{
		/* Emoticon or UserDisplay */
		char *content;
		gsize len;
		MsnSlpSession *slpsession;
		MsnSlpLink *slplink;
		MsnSlpMessage *slpmsg;
		MsnObject *obj;
		char *msnobj_data;
		PurpleStoredImage *img;
		int type;

		/* Send Ok */
		content = g_strdup_printf("SessionID: %lu\r\n\r\n",
								  slpcall->session_id);

		send_ok(slpcall, branch, "application/x-msnmsgr-sessionreqbody",
				content);

		g_free(content);

		slplink = slpcall->slplink;

		msnobj_data = (char *)purple_base64_decode(context, &len);
		obj = msn_object_new_from_string(msnobj_data);
		type = msn_object_get_type(obj);
		g_free(msnobj_data);

		if (!(type == MSN_OBJECT_USERTILE))
		{
			purple_debug_error("msn", "Wrong object?\n");
			msn_object_destroy(obj);
			g_return_if_reached();
		}

		img = msn_object_get_image(obj);
		if (img == NULL)
		{
			purple_debug_error("msn", "Wrong object.\n");
			msn_object_destroy(obj);
			g_return_if_reached();
		}

		msn_object_destroy(obj);

		slpsession = msn_slplink_find_slp_session(slplink,
												  slpcall->session_id);

		/* DATA PREP */
		slpmsg = msn_slpmsg_new(slplink);
		slpmsg->slpcall = slpcall;
		slpmsg->slpsession = slpsession;
		slpmsg->session_id = slpsession->id;
		msn_slpmsg_set_body(slpmsg, NULL, 4);
#ifdef MSN_DEBUG_SLP
		slpmsg->info = "SLP DATA PREP";
#endif
		msn_slplink_queue_slpmsg(slplink, slpmsg);

		/* DATA */
		slpmsg = msn_slpmsg_new(slplink);
		slpmsg->slpcall = slpcall;
		slpmsg->slpsession = slpsession;
		slpmsg->flags = 0x20;
#ifdef MSN_DEBUG_SLP
		slpmsg->info = "SLP DATA";
#endif
		msn_slpmsg_set_image(slpmsg, img);
		msn_slplink_queue_slpmsg(slplink, slpmsg);
	}
	else if (!strcmp(euf_guid, "5D3E02AB-6190-11D3-BBBB-00C04F795683"))
	{
		/* File Transfer */
		PurpleAccount *account;
		PurpleXfer *xfer;
		char *bin;
		gsize bin_len;
		guint32 file_size;
		char *file_name;
		gunichar2 *uni_name;

		account = slpcall->slplink->session->account;

		slpcall->cb = msn_xfer_completed_cb;
		slpcall->end_cb = msn_xfer_end_cb;
		slpcall->progress_cb = msn_xfer_progress_cb;
		slpcall->branch = g_strdup(branch);

		slpcall->pending = TRUE;

		xfer = purple_xfer_new(account, PURPLE_XFER_RECEIVE,
							 slpcall->slplink->remote_user);
		if (xfer)
		{
			bin = (char *)purple_base64_decode(context, &bin_len);
			file_size = GUINT32_FROM_LE(*(gsize *)(bin + 8));

			uni_name = (gunichar2 *)(bin + 20);
			while(*uni_name != 0 && ((char *)uni_name - (bin + 20)) < MAX_FILE_NAME_LEN) {
				*uni_name = GUINT16_FROM_LE(*uni_name);
				uni_name++;
			}

			file_name = g_utf16_to_utf8((const gunichar2 *)(bin + 20), -1,
										NULL, NULL, NULL);

			g_free(bin);

			purple_xfer_set_filename(xfer, file_name);
			purple_xfer_set_size(xfer, file_size);
			purple_xfer_set_init_fnc(xfer, msn_xfer_init);
			purple_xfer_set_request_denied_fnc(xfer, msn_xfer_cancel);
			purple_xfer_set_cancel_recv_fnc(xfer, msn_xfer_cancel);

			slpcall->xfer = xfer;
			purple_xfer_ref(slpcall->xfer);

			xfer->data = slpcall;

			purple_xfer_request(xfer);
		}
	}
}

void
send_bye(MsnSlpCall *slpcall, const char *type)
{
	MsnSlpLink *slplink;
	MsnSlpMessage *slpmsg;
	char *header;

	slplink = slpcall->slplink;

	g_return_if_fail(slplink != NULL);

	header = g_strdup_printf("BYE MSNMSGR:%s MSNSLP/1.0",
							 slplink->local_user);

	slpmsg = msn_slpmsg_sip_new(slpcall, 0, header,
								"A0D624A6-6C0C-4283-A9E0-BC97B4B46D32",
								type,
								"\r\n");
	g_free(header);

#ifdef MSN_DEBUG_SLP
	slpmsg->info = "SLP BYE";
	slpmsg->text_body = TRUE;
#endif

	msn_slplink_queue_slpmsg(slplink, slpmsg);
}

static void
got_invite(MsnSlpCall *slpcall,
		   const char *branch, const char *type, const char *content)
{
	MsnSlpLink *slplink;

	slplink = slpcall->slplink;

	if (!strcmp(type, "application/x-msnmsgr-sessionreqbody"))
	{
		char *euf_guid, *context;
		char *temp;

		euf_guid = get_token(content, "EUF-GUID: {", "}\r\n");

		temp = get_token(content, "SessionID: ", "\r\n");
		if (temp != NULL)
			slpcall->session_id = atoi(temp);
		g_free(temp);

		temp = get_token(content, "AppID: ", "\r\n");
		if (temp != NULL)
			slpcall->app_id = atoi(temp);
		g_free(temp);

		context = get_token(content, "Context: ", "\r\n");

		if (context != NULL)
			got_sessionreq(slpcall, branch, euf_guid, context);

		g_free(context);
		g_free(euf_guid);
	}
	else if (!strcmp(type, "application/x-msnmsgr-transreqbody"))
	{
		/* A direct connection? */

		char *listening, *nonce;
		char *content;

		if (FALSE)
		{
#if 0
			MsnDirectConn *directconn;
			/* const char *ip_addr; */
			char *ip_port;
			int port;

			/* ip_addr = purple_prefs_get_string("/purple/ft/public_ip"); */
			ip_port = "5190";
			listening = "true";
			nonce = rand_guid();

			directconn = msn_directconn_new(slplink);

			/* msn_directconn_parse_nonce(directconn, nonce); */
			directconn->nonce = g_strdup(nonce);

			msn_directconn_listen(directconn);

			port = directconn->port;

			content = g_strdup_printf(
				"Bridge: TCPv1\r\n"
				"Listening: %s\r\n"
				"Nonce: {%s}\r\n"
				"Ipv4Internal-Addrs: 192.168.0.82\r\n"
				"Ipv4Internal-Port: %d\r\n"
				"\r\n",
				listening,
				nonce,
				port);
#endif
		}
		else
		{
			listening = "false";
			nonce = g_strdup("00000000-0000-0000-0000-000000000000");

			content = g_strdup_printf(
				"Bridge: TCPv1\r\n"
				"Listening: %s\r\n"
				"Nonce: {%s}\r\n"
				"\r\n",
				listening,
				nonce);
		}

		send_ok(slpcall, branch,
				"application/x-msnmsgr-transrespbody", content);

		g_free(content);
		g_free(nonce);
	}
	else if (!strcmp(type, "application/x-msnmsgr-transrespbody"))
	{
#if 0
		char *ip_addrs;
		char *temp;
		char *nonce;
		int port;

		nonce = get_token(content, "Nonce: {", "}\r\n");
		ip_addrs = get_token(content, "IPv4Internal-Addrs: ", "\r\n");

		temp = get_token(content, "IPv4Internal-Port: ", "\r\n");
		if (temp != NULL)
			port = atoi(temp);
		else
			port = -1;
		g_free(temp);

		if (ip_addrs == NULL)
			return;

		if (port > 0)
			got_transresp(slpcall, nonce, ip_addrs, port);

		g_free(nonce);
		g_free(ip_addrs);
#endif
	}
}

static void
got_ok(MsnSlpCall *slpcall,
	   const char *type, const char *content)
{
	g_return_if_fail(slpcall != NULL);
	g_return_if_fail(type    != NULL);

	if (!strcmp(type, "application/x-msnmsgr-sessionreqbody"))
	{
#if 0
		if (slpcall->type == MSN_SLPCALL_DC)
		{
			/* First let's try a DirectConnection. */

			MsnSlpLink *slplink;
			MsnSlpMessage *slpmsg;
			char *header;
			char *content;
			char *branch;

			slplink = slpcall->slplink;

			branch = rand_guid();

			content = g_strdup_printf(
				"Bridges: TRUDPv1 TCPv1\r\n"
				"NetID: 0\r\n"
				"Conn-Type: Direct-Connect\r\n"
				"UPnPNat: false\r\n"
				"ICF: false\r\n"
			);

			header = g_strdup_printf("INVITE MSNMSGR:%s MSNSLP/1.0",
									 slplink->remote_user);

			slpmsg = msn_slp_sipmsg_new(slpcall, 0, header, branch,
										"application/x-msnmsgr-transreqbody",
										content);

#ifdef MSN_DEBUG_SLP
			slpmsg->info = "SLP INVITE";
			slpmsg->text_body = TRUE;
#endif
			msn_slplink_send_slpmsg(slplink, slpmsg);

			g_free(header);
			g_free(content);

			g_free(branch);
		}
		else
		{
			msn_slp_call_session_init(slpcall);
		}
#else
		msn_slp_call_session_init(slpcall);
#endif
	}
	else if (!strcmp(type, "application/x-msnmsgr-transreqbody"))
	{
		/* Do we get this? */
		purple_debug_info("msn", "OK with transreqbody\n");
	}
	else if (!strcmp(type, "application/x-msnmsgr-transrespbody"))
	{
#if 0
		char *ip_addrs;
		char *temp;
		char *nonce;
		int port;

		nonce = get_token(content, "Nonce: {", "}\r\n");
		ip_addrs = get_token(content, "IPv4Internal-Addrs: ", "\r\n");

		temp = get_token(content, "IPv4Internal-Port: ", "\r\n");
		if (temp != NULL)
			port = atoi(temp);
		else
			port = -1;
		g_free(temp);

		if (ip_addrs == NULL)
			return;

		if (port > 0)
			got_transresp(slpcall, nonce, ip_addrs, port);

		g_free(nonce);
		g_free(ip_addrs);
#endif
	}
}

MsnSlpCall *
msn_slp_sip_recv(MsnSlpLink *slplink, const char *body)
{
	MsnSlpCall *slpcall;

	if (body == NULL)
	{
		purple_debug_warning("msn", "received bogus message\n");
		return NULL;
	}

	if (!strncmp(body, "INVITE", strlen("INVITE")))
	{
		char *branch;
		char *content;
		char *content_type;

		slpcall = msn_slp_call_new(slplink);

		/* From: <msnmsgr:buddy@hotmail.com> */
#if 0
		slpcall->remote_user = get_token(body, "From: <msnmsgr:", ">\r\n");
#endif

		branch = get_token(body, ";branch={", "}");

		slpcall->id = get_token(body, "Call-ID: {", "}");

#if 0
		long content_len = -1;

		temp = get_token(body, "Content-Length: ", "\r\n");
		if (temp != NULL)
			content_len = atoi(temp);
		g_free(temp);
#endif
		content_type = get_token(body, "Content-Type: ", "\r\n");

		content = get_token(body, "\r\n\r\n", NULL);

		got_invite(slpcall, branch, content_type, content);

		g_free(branch);
		g_free(content_type);
		g_free(content);
	}
	else if (!strncmp(body, "MSNSLP/1.0 ", strlen("MSNSLP/1.0 ")))
	{
		char *content;
		char *content_type;
		/* Make sure this is "OK" */
		const char *status = body + strlen("MSNSLP/1.0 ");
		char *call_id;

		call_id = get_token(body, "Call-ID: {", "}");
		slpcall = msn_slplink_find_slp_call(slplink, call_id);
		g_free(call_id);

		g_return_val_if_fail(slpcall != NULL, NULL);

		if (strncmp(status, "200 OK", 6))
		{
			/* It's not valid. Kill this off. */
			char temp[32];
			const char *c;

			/* Eww */
			if ((c = strchr(status, '\r')) || (c = strchr(status, '\n')) ||
				(c = strchr(status, '\0')))
			{
				size_t offset =  c - status;
				if (offset >= sizeof(temp))
					offset = sizeof(temp) - 1;

				strncpy(temp, status, offset);
				temp[offset] = '\0';
			}

			purple_debug_error("msn", "Received non-OK result: %s\n", temp);

			slpcall->wasted = TRUE;

			/* msn_slp_call_destroy(slpcall); */
			return slpcall;
		}

		content_type = get_token(body, "Content-Type: ", "\r\n");

		content = get_token(body, "\r\n\r\n", NULL);

		got_ok(slpcall, content_type, content);

		g_free(content_type);
		g_free(content);
	}
	else if (!strncmp(body, "BYE", strlen("BYE")))
	{
		char *call_id;

		call_id = get_token(body, "Call-ID: {", "}");
		slpcall = msn_slplink_find_slp_call(slplink, call_id);
		g_free(call_id);

		if (slpcall != NULL)
			slpcall->wasted = TRUE;

		/* msn_slp_call_destroy(slpcall); */
	}
	else
		slpcall = NULL;

	return slpcall;
}

/**************************************************************************
 * Msg Callbacks
 **************************************************************************/

void
msn_p2p_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnSession *session;
	MsnSlpLink *slplink;

	session = cmdproc->servconn->session;
	slplink = msn_session_get_slplink(session, msg->remote_user);

	if (slplink->swboard == NULL)
	{
		/* We will need this in order to change its flags. */
		slplink->swboard = (MsnSwitchBoard *)cmdproc->data;
		/* If swboard is NULL, something has probably gone wrong earlier on
		 * I didn't want to do this, but MSN 7 is somehow causing us to crash
		 * here, I couldn't reproduce it to debug more, and people are
		 * reporting bugs. Hopefully this doesn't cause more crashes. Stu.
		 */
		if (slplink->swboard != NULL)
			slplink->swboard->slplinks = g_list_prepend(slplink->swboard->slplinks, slplink);
		else
			purple_debug_error("msn", "msn_p2p_msg, swboard is NULL, ouch!\n");
	}

	msn_slplink_process_msg(slplink, msg);
}

static void
got_emoticon(MsnSlpCall *slpcall,
			 const guchar *data, gsize size)
{

	PurpleConversation *conv;
	PurpleConnection *gc;
	const char *who;

	gc = slpcall->slplink->session->account->gc;
	who = slpcall->slplink->remote_user;

	if ((conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, who, gc->account))) {

		/* FIXME: it would be better if we wrote the data as we received it
		          instead of all at once, calling write multiple times and
		          close once at the very end
		*/
		purple_conv_custom_smiley_write(conv, slpcall->data_info, data, size);
		purple_conv_custom_smiley_close(conv, slpcall->data_info);
	}
#ifdef MSN_DEBUG_UD
	purple_debug_info("msn", "Got smiley: %s\n", slpcall->data_info);
#endif
}

void
msn_emoticon_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnSession *session;
	MsnSlpLink *slplink;
	MsnObject *obj;
	char **tokens;
	char *smile, *body_str;
	const char *body, *who, *sha1;
	guint tok;
	size_t body_len;

	PurpleConversation *conv;

	session = cmdproc->servconn->session;

	if (!purple_account_get_bool(session->account, "custom_smileys", TRUE))
		return;

	body = msn_message_get_bin_data(msg, &body_len);
	body_str = g_strndup(body, body_len);

	/* MSN Messenger 7 may send more than one MSNObject in a single message...
	 * Maybe 10 tokens is a reasonable max value. */
	tokens = g_strsplit(body_str, "\t", 10);

	g_free(body_str);

	for (tok = 0; tok < 9; tok += 2) {
		if (tokens[tok] == NULL || tokens[tok + 1] == NULL) {
			break;
		}

		smile = tokens[tok];
		obj = msn_object_new_from_string(purple_url_decode(tokens[tok + 1]));

		if (obj == NULL)
			break;

		who = msn_object_get_creator(obj);
		sha1 = msn_object_get_sha1(obj);

		slplink = msn_session_get_slplink(session, who);

		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, who,
												   session->account);

		/* If the conversation doesn't exist then this is a custom smiley
		 * used in the first message in a MSN conversation: we need to create
		 * the conversation now, otherwise the custom smiley won't be shown.
		 * This happens because every GtkIMHtml has its own smiley tree: if
		 * the conversation doesn't exist then we cannot associate the new
		 * smiley with its GtkIMHtml widget. */
		if (!conv) {
			conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, session->account, who);
		}

		if (purple_conv_custom_smiley_add(conv, smile, "sha1", sha1, TRUE)) {
			msn_slplink_request_object(slplink, smile, got_emoticon, NULL, obj);
		}

		msn_object_destroy(obj);
		obj =   NULL;
		who =   NULL;
		sha1 = NULL;
	}
	g_strfreev(tokens);
}

static gboolean
buddy_icon_cached(PurpleConnection *gc, MsnObject *obj)
{
	PurpleAccount *account;
	PurpleBuddy *buddy;
	const char *old;
	const char *new;

	g_return_val_if_fail(obj != NULL, FALSE);

	account = purple_connection_get_account(gc);

	buddy = purple_find_buddy(account, msn_object_get_creator(obj));
	if (buddy == NULL)
		return FALSE;

	old = purple_buddy_icons_get_checksum_for_user(buddy);
	new = msn_object_get_sha1(obj);

	if (new == NULL)
		return FALSE;

	/* If the old and new checksums are the same, and the file actually exists,
	 * then return TRUE */
	if (old != NULL && !strcmp(old, new))
		return TRUE;

	return FALSE;
}

static void
msn_release_buddy_icon_request(MsnUserList *userlist)
{
	MsnUser *user;

	g_return_if_fail(userlist != NULL);

#ifdef MSN_DEBUG_UD
	purple_debug_info("msn", "Releasing buddy icon request\n");
#endif

	if (userlist->buddy_icon_window > 0)
	{
		GQueue *queue;
		PurpleAccount *account;
		const char *username;

		queue = userlist->buddy_icon_requests;

		if (g_queue_is_empty(userlist->buddy_icon_requests))
			return;

		user = g_queue_pop_head(queue);

		account  = userlist->session->account;
		username = user->passport;

		userlist->buddy_icon_window--;
		msn_request_user_display(user);

#ifdef MSN_DEBUG_UD
		purple_debug_info("msn", "msn_release_buddy_icon_request(): buddy_icon_window-- yields =%d\n",
						userlist->buddy_icon_window);
#endif
	}
}

/*
 * Called on a timeout from end_user_display(). Frees a buddy icon window slow and dequeues the next
 * buddy icon request if there is one.
 */
static gboolean
msn_release_buddy_icon_request_timeout(gpointer data)
{
	MsnUserList *userlist = (MsnUserList *)data;
	
	/* Free one window slot */
	userlist->buddy_icon_window++;	
	
	/* Clear the tag for our former request timer */
	userlist->buddy_icon_request_timer = 0;
	
	msn_release_buddy_icon_request(userlist);
	
	return FALSE;
}

void
msn_queue_buddy_icon_request(MsnUser *user)
{
	PurpleAccount *account;
	MsnObject *obj;
	GQueue *queue;

	g_return_if_fail(user != NULL);

	account = user->userlist->session->account;

	obj = msn_user_get_object(user);

	if (obj == NULL)
	{
		purple_buddy_icons_set_for_user(account, user->passport, NULL, 0, NULL);
		return;
	}

	if (!buddy_icon_cached(account->gc, obj))
	{
		MsnUserList *userlist;

		userlist = user->userlist;
		queue = userlist->buddy_icon_requests;

#ifdef MSN_DEBUG_UD
		purple_debug_info("msn", "Queueing buddy icon request for %s (buddy_icon_window = %i)\n",
						user->passport, userlist->buddy_icon_window);
#endif

		g_queue_push_tail(queue, user);

		if (userlist->buddy_icon_window > 0)
			msn_release_buddy_icon_request(userlist);
	}
}

static void
got_user_display(MsnSlpCall *slpcall,
				 const guchar *data, gsize size)
{
	MsnUserList *userlist;
	const char *info;
	PurpleAccount *account;

	g_return_if_fail(slpcall != NULL);

	info = slpcall->data_info;
#ifdef MSN_DEBUG_UD
	purple_debug_info("msn", "Got User Display: %s\n", slpcall->slplink->remote_user);
#endif

	userlist = slpcall->slplink->session->userlist;
	account = slpcall->slplink->session->account;

	purple_buddy_icons_set_for_user(account, slpcall->slplink->remote_user,
								  g_memdup(data, size), size, info);

#if 0
	/* Free one window slot */
	userlist->buddy_icon_window++;

	purple_debug_info("msn", "got_user_display(): buddy_icon_window++ yields =%d\n",
					userlist->buddy_icon_window);

	msn_release_buddy_icon_request(userlist);
#endif
}

static void
end_user_display(MsnSlpCall *slpcall, MsnSession *session)
{
	MsnUserList *userlist;

	g_return_if_fail(session != NULL);

#ifdef MSN_DEBUG_UD
	purple_debug_info("msn", "End User Display\n");
#endif

	userlist = session->userlist;

	/* If the session is being destroyed we better stop doing anything. */
	if (session->destroying)
		return;

	/* Delay before freeing a buddy icon window slot and requesting the next icon, if appropriate.
	 * If we don't delay, we'll rapidly hit the MSN equivalent of AIM's rate limiting; the server will
	 * send us an error 800 like so:
	 *
	 * C: NS 000: XFR 21 SB
	 * S: NS 000: 800 21
	 */
	if (userlist->buddy_icon_request_timer) {
		/* Free the window slot used by this previous request */
		userlist->buddy_icon_window++;

		/* Clear our pending timeout */
		purple_timeout_remove(userlist->buddy_icon_request_timer);
	}

	/* Wait BUDDY_ICON_DELAY ms before freeing our window slot and requesting the next icon. */
	userlist->buddy_icon_request_timer = purple_timeout_add(BUDDY_ICON_DELAY, 
														  msn_release_buddy_icon_request_timeout, userlist);
}

void
msn_request_user_display(MsnUser *user)
{
	PurpleAccount *account;
	MsnSession *session;
	MsnSlpLink *slplink;
	MsnObject *obj;
	const char *info;

	session = user->userlist->session;
	account = session->account;

	slplink = msn_session_get_slplink(session, user->passport);

	obj = msn_user_get_object(user);

	info = msn_object_get_sha1(obj);

	if (g_ascii_strcasecmp(user->passport,
						   purple_account_get_username(account)))
	{
		msn_slplink_request_object(slplink, info, got_user_display,
								   end_user_display, obj);
	}
	else
	{
		MsnObject *my_obj = NULL;
		gconstpointer data = NULL;
		size_t len = 0;

#ifdef MSN_DEBUG_UD
		purple_debug_info("msn", "Requesting our own user display\n");
#endif

		my_obj = msn_user_get_object(session->user);

		if (my_obj != NULL)
		{
			PurpleStoredImage *img = msn_object_get_image(my_obj);
			data = purple_imgstore_get_data(img);
			len = purple_imgstore_get_size(img);
		}

		purple_buddy_icons_set_for_user(account, user->passport, g_memdup(data, len), len, info);

		/* Free one window slot */
		session->userlist->buddy_icon_window++;

#ifdef MSN_DEBUG_UD
		purple_debug_info("msn", "msn_request_user_display(): buddy_icon_window++ yields =%d\n",
						session->userlist->buddy_icon_window);
#endif

		msn_release_buddy_icon_request(session->userlist);
	}
}
