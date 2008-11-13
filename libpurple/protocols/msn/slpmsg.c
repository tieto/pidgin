/**
 * @file slpmsg.h SLP Message functions
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
#include "slpmsg.h"
#include "slplink.h"

/**************************************************************************
 * SLP Message
 **************************************************************************/

MsnSlpMessage *
msn_slpmsg_new(MsnSlpLink *slplink)
{
	MsnSlpMessage *slpmsg;

	slpmsg = g_new0(MsnSlpMessage, 1);

#ifdef MSN_DEBUG_SLPMSG
	purple_debug_info("msn", "slpmsg new (%p)\n", slpmsg);
#endif

	slpmsg->slplink = slplink;

	slplink->slp_msgs =
		g_list_append(slplink->slp_msgs, slpmsg);

	return slpmsg;
}

void
msn_slpmsg_destroy(MsnSlpMessage *slpmsg)
{
	MsnSlpLink *slplink;
	GList *cur;

	g_return_if_fail(slpmsg != NULL);

#ifdef MSN_DEBUG_SLPMSG
	purple_debug_info("msn", "slpmsg destroy (%p)\n", slpmsg);
#endif

	slplink = slpmsg->slplink;

	if (slpmsg->fp != NULL)
		fclose(slpmsg->fp);

	purple_imgstore_unref(slpmsg->img);

	/* We don't want to free the data of the PurpleStoredImage,
	 * but to avoid code duplication, it's sharing buffer. */
	if (slpmsg->img == NULL)
		g_free(slpmsg->buffer);

#ifdef MSN_DEBUG_SLP
	/*
	if (slpmsg->info != NULL)
		g_free(slpmsg->info);
	*/
#endif

	for (cur = slpmsg->msgs; cur != NULL; cur = cur->next)
	{
		/* Something is pointing to this slpmsg, so we should remove that
		 * pointer to prevent a crash. */
		/* Ex: a user goes offline and after that we receive an ACK */

		MsnMessage *msg = cur->data;

#ifdef MSN_DEBUG_SLPMSG
		purple_debug_info("msn", "Unlink slpmsg callbacks.\n");
#endif

		msg->ack_cb = NULL;
		msg->nak_cb = NULL;
		msg->ack_data = NULL;
	}
	g_list_free(slpmsg->msgs);

	slplink->slp_msgs = g_list_remove(slplink->slp_msgs, slpmsg);

	g_free(slpmsg);
}

void
msn_slpmsg_set_body(MsnSlpMessage *slpmsg, const char *body,
						 long long size)
{
	/* We can only have one data source at a time. */
	g_return_if_fail(slpmsg->buffer == NULL);
	g_return_if_fail(slpmsg->img == NULL);
	g_return_if_fail(slpmsg->fp == NULL);

	if (body != NULL)
		slpmsg->buffer = g_memdup(body, size);
	else
		slpmsg->buffer = g_new0(guchar, size);

	slpmsg->size = size;
}

void
msn_slpmsg_set_image(MsnSlpMessage *slpmsg, PurpleStoredImage *img)
{
	/* We can only have one data source at a time. */
	g_return_if_fail(slpmsg->buffer == NULL);
	g_return_if_fail(slpmsg->img == NULL);
	g_return_if_fail(slpmsg->fp == NULL);

	slpmsg->img = purple_imgstore_ref(img);
	slpmsg->buffer = (guchar *)purple_imgstore_get_data(img);
	slpmsg->size = purple_imgstore_get_size(img);
}

#ifdef MSN_DEBUG_SLP
void
msn_slpmsg_show(MsnMessage *msg)
{
	const char *info;
	gboolean text;
	guint32 flags;

	text = FALSE;

	flags = GUINT32_TO_LE(msg->msnslp_header.flags);

	switch (flags)
	{
		case 0x0:
			info = "SLP CONTROL";
			text = TRUE;
			break;
		case 0x2:
			info = "SLP ACK"; break;
		case 0x20:
		case 0x1000030:
			info = "SLP DATA"; break;
		default:
			info = "SLP UNKNOWN"; break;
	}

	msn_message_show_readable(msg, info, text);
}
#endif

MsnSlpMessage *
msn_slpmsg_sip_new(MsnSlpCall *slpcall, int cseq,
				   const char *header, const char *branch,
				   const char *content_type, const char *content)
{
	MsnSlpLink *slplink;
	PurpleAccount *account;
	MsnSlpMessage *slpmsg;
	char *body;
	gsize body_len;
	gsize content_len;

	g_return_val_if_fail(slpcall != NULL, NULL);
	g_return_val_if_fail(header  != NULL, NULL);

	slplink = slpcall->slplink;
	account = slplink->session->account;

	/* Let's remember that "content" should end with a 0x00 */

	content_len = (content != NULL) ? strlen(content) + 1 : 0;

	body = g_strdup_printf(
		"%s\r\n"
		"To: <msnmsgr:%s>\r\n"
		"From: <msnmsgr:%s>\r\n"
		"Via: MSNSLP/1.0/TLP ;branch={%s}\r\n"
		"CSeq: %d\r\n"
		"Call-ID: {%s}\r\n"
		"Max-Forwards: 0\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %" G_GSIZE_FORMAT "\r\n"
		"\r\n",
		header,
		slplink->remote_user,
		purple_account_get_username(account),
		branch,
		cseq,
		slpcall->id,
		content_type,
		content_len);

	body_len = strlen(body);

	if (content_len > 0)
	{
		body_len += content_len;
		body = g_realloc(body, body_len);
		g_strlcat(body, content, body_len);
	}

	slpmsg = msn_slpmsg_new(slplink);
	msn_slpmsg_set_body(slpmsg, body, body_len);

	slpmsg->sip = TRUE;
	slpmsg->slpcall = slpcall;

	g_free(body);

	return slpmsg;
}
