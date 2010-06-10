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

#include "internal.h"
#include "debug.h"

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

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "slpmsg new (%p)\n", slpmsg);

	if (slplink) 
		msn_slpmsg_set_slplink(slpmsg, slplink);
	else
		slpmsg->slplink = NULL;

	slpmsg->header = NULL;
	slpmsg->footer = NULL;

	return slpmsg;
}

MsnSlpMessage *msn_slpmsg_new_from_data(const char *data, size_t data_len)
{
	MsnSlpMessage *slpmsg;
	MsnP2PHeader *header;
	const char *tmp;
	int body_len;

	tmp = data;
	slpmsg = msn_slpmsg_new(NULL);

	if (data_len < sizeof(*header)) {
		return NULL;
	}

	/* Extract the binary SLP header */
	slpmsg->header = msn_p2p_header_from_wire((MsnP2PHeader*)tmp);

	/* Extract the body */
	body_len = data_len - (tmp - data);
	/* msg->body_len = msg->msnslp_header.length; */

	if (body_len > 0) {
		slpmsg->size = body_len;
		slpmsg->buffer = g_malloc(body_len);
		memcpy(slpmsg->buffer, tmp, body_len);
		tmp += body_len;
	}

	/* Extract the footer */
	if (body_len >= 0) 
		slpmsg->footer = msn_p2p_footer_from_wire((MsnP2PFooter*)tmp);

	return slpmsg;
}

void
msn_slpmsg_destroy(MsnSlpMessage *slpmsg)
{
	MsnSlpLink *slplink;
	GList *cur;

	g_return_if_fail(slpmsg != NULL);

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "slpmsg destroy (%p)\n", slpmsg);

	slplink = slpmsg->slplink;

	purple_imgstore_unref(slpmsg->img);

	/* We don't want to free the data of the PurpleStoredImage,
	 * but to avoid code duplication, it's sharing buffer. */
	if (slpmsg->img == NULL)
		g_free(slpmsg->buffer);

	for (cur = slpmsg->msgs; cur != NULL; cur = g_list_delete_link(cur, cur))
	{
		/* Something is pointing to this slpmsg, so we should remove that
		 * pointer to prevent a crash. */
		/* Ex: a user goes offline and after that we receive an ACK */

		MsnMessage *msg = cur->data;

		msg->ack_cb = NULL;
		msg->nak_cb = NULL;
		msg->ack_data = NULL;
		msn_message_unref(msg);
	}

	slplink->slp_msgs = g_list_remove(slplink->slp_msgs, slpmsg);

	g_free(slpmsg->header);
	g_free(slpmsg->footer);

	g_free(slpmsg);
}

void
msn_slpmsg_set_slplink(MsnSlpMessage *slpmsg, MsnSlpLink *slplink)
{
	g_return_if_fail(slplink != NULL);

	slpmsg->slplink = slplink;

	slplink->slp_msgs =
		g_list_append(slplink->slp_msgs, slpmsg);

}

void
msn_slpmsg_set_body(MsnSlpMessage *slpmsg, const char *body,
						 long long size)
{
	/* We can only have one data source at a time. */
	g_return_if_fail(slpmsg->buffer == NULL);
	g_return_if_fail(slpmsg->img == NULL);
	g_return_if_fail(slpmsg->ft == FALSE);

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
	g_return_if_fail(slpmsg->ft == FALSE);

	slpmsg->img = purple_imgstore_ref(img);
	slpmsg->buffer = (guchar *)purple_imgstore_get_data(img);
	slpmsg->size = purple_imgstore_get_size(img);
}

void
msn_slpmsg_show(MsnMessage *msg)
{
	const char *info;
	gboolean text;
	guint32 flags;

	text = FALSE;

	flags = GUINT32_TO_LE(msg->slpmsg->header->flags);

	switch (flags)
	{
		case P2P_NO_FLAG :
			info = "SLP CONTROL";
			text = TRUE;
			break;
		case P2P_ACK:
			info = "SLP ACK"; break;
		case P2P_MSN_OBJ_DATA:
		case P2P_FILE_DATA:
			info = "SLP DATA"; break;
		default:
			info = "SLP UNKNOWN"; break;
	}

	msn_message_show_readable(msg, info, text);
}

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

MsnSlpMessage *msn_slpmsg_new_ack(MsnP2PHeader *header)
{
	MsnSlpMessage *slpmsg;

	slpmsg = msn_slpmsg_new(NULL);

	slpmsg->session_id = header->session_id;
	slpmsg->size       = header->total_size;
	slpmsg->flags      = P2P_ACK;
	slpmsg->ack_id     = header->id;
	slpmsg->ack_sub_id = header->ack_id;
	slpmsg->ack_size   = header->total_size;
	slpmsg->info = "SLP ACK";

	return slpmsg;
}

char *msn_slpmsg_serialize(MsnSlpMessage *slpmsg, size_t *ret_size)
{
	MsnP2PHeader *header;
	MsnP2PFooter *footer;
	char *base;
	char *tmp;
	size_t siz;

	base = g_malloc(P2P_PACKET_HEADER_SIZE + slpmsg->size + sizeof(MsnP2PFooter));
	tmp = base;

	header = msn_p2p_header_to_wire(slpmsg->header);
	footer = msn_p2p_footer_to_wire(slpmsg->footer);

	siz = sizeof(MsnP2PHeader);
	/* Copy header */
	memcpy(tmp, (char*)header, siz);
	tmp += siz;

	/* Copy body */
	memcpy(tmp, slpmsg->buffer, slpmsg->size);
	tmp += slpmsg->size;

	/* Copy footer */
	siz = sizeof(MsnP2PFooter);
	memcpy(tmp, (char*)footer, siz);
	tmp += siz;

	*ret_size = tmp - base;

	return base;
}
