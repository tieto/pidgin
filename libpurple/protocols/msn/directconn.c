/**
 * @file directconn.c MSN direct connection functions
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
#include "directconn.h"

#include "slp.h"
#include "slpmsg.h"

#pragma pack(push,1)
typedef struct {
	guint32 session_id;
	guint32 seq_id;
	guint64 offset;
	guint64 total_size;
	guint32 length;
	guint32 flags;
	guint32 ack_id;
	guint32 ack_uid;
	guint64 ack_size;
/*	guint8  body[1]; */
} MsnDcContext;
#pragma pack(pop)

#define DC_PACKET_HEADER_SIZE sizeof(MsnDcContext)
#define DC_MAX_BODY_SIZE      1352
#define DC_MAX_PACKET_SIZE    (DC_PACKET_HEADER_SIZE + DC_MAX_BODY_SIZE)

static void
msn_dc_calculate_nonce_hash(MsnDirectConnNonceType type,
                            const guchar nonce[16], gchar nonce_hash[37])
{
	guchar digest[20];

	if (type == DC_NONCE_SHA1) {
		PurpleCipher *cipher = purple_ciphers_find_cipher("sha1");
		PurpleCipherContext *context = purple_cipher_context_new(cipher, NULL);
		purple_cipher_context_append(context, nonce, sizeof(nonce));
		purple_cipher_context_digest(context, sizeof(digest), digest, NULL);
		purple_cipher_context_destroy(context);
	} else if (type == DC_NONCE_PLAIN) {
		memcpy(digest, nonce, 16);
	}

	g_sprintf(nonce_hash,
	          "%08X-%04X-%04X-%04X-%08X%04X",
	          GUINT32_FROM_LE(*((guint32 *)(digest + 0))),
	          GUINT16_FROM_LE(*((guint16 *)(digest + 4))),
	          GUINT16_FROM_LE(*((guint16 *)(digest + 6))),
	          GUINT16_FROM_BE(*((guint16 *)(digest + 8))),
	          GUINT32_FROM_BE(*((guint32 *)(digest + 10))),
	          GUINT16_FROM_BE(*((guint16 *)(digest + 14)))
	);
}

static void
msn_dc_generate_nonce(MsnDirectConn *dc)
{
	guint32 *nonce;
	int i;

	nonce = (guint32 *)&dc->nonce;
	for (i = 0; i < 4; i++)
		nonce[i] = rand();

	msn_dc_calculate_nonce_hash(dc->nonce_type, dc->nonce, dc->nonce_hash);

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "DC %p generated nonce %s\n", dc, dc->nonce_hash);
}

static MsnDirectConnPacket *
msn_dc_new_packet(guint32 length)
{
	MsnDirectConnPacket	*p;

	p = g_new0(MsnDirectConnPacket, 1);
	p->length = length;
	p->data = g_malloc(length);

	return p;
}

static void
msn_dc_destroy_packet(MsnDirectConnPacket *p)
{
	g_free(p->data);

	if (p->msg)
		msn_message_unref(p->msg);

	g_free(p);
}

MsnDirectConn *
msn_dc_new(MsnSlpCall *slpcall)
{
	MsnDirectConn *dc;

	g_return_val_if_fail(slpcall != NULL, NULL);

	dc = g_new0(MsnDirectConn, 1);

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "msn_dc_new %p\n", dc);

	dc->slplink = slpcall->slplink;
	dc->slpcall = slpcall;

	if (dc->slplink->dc != NULL)
		purple_debug_warning("msn", "msn_dc_new: slplink already has an allocated DC!\n");

	dc->slplink->dc = dc;

	dc->msg_body = NULL;
	dc->prev_ack = NULL;
	dc->listen_data = NULL;
	dc->connect_data = NULL;
	dc->listenfd = -1;
	dc->listenfd_handle = 0;
	dc->connect_timeout_handle = 0;
	dc->fd = -1;
	dc->recv_handle = 0;
	dc->send_handle = 0;
	dc->state = DC_STATE_CLOSED;
	dc->in_buffer = NULL;
	dc->out_queue = g_queue_new();
	dc->msg_pos = -1;
	dc->send_connection_info_msg_cb = NULL;
	dc->ext_ip = NULL;
	dc->timeout_handle = 0;
	dc->progress = FALSE;
	//dc->num_calls = 1;

	/* TODO: Probably should set this based on buddy caps */
	dc->nonce_type = DC_NONCE_PLAIN;
	msn_dc_generate_nonce(dc);

	return dc;
}

void
msn_dc_destroy(MsnDirectConn *dc)
{
	MsnSlpLink *slplink;

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "msn_dc_destroy %p\n", dc);

	g_return_if_fail(dc != NULL);

	slplink = dc->slplink;

	if (dc->slpcall != NULL)
		dc->slpcall->wait_for_socket = FALSE;

	slplink->dc = NULL;

	if (slplink->swboard == NULL)
		msn_slplink_destroy(slplink);

	g_free(dc->msg_body);

	if (dc->prev_ack) {
		msn_slpmsg_destroy(dc->prev_ack);
	}

	if (dc->listen_data != NULL) {
		purple_network_listen_cancel(dc->listen_data);
	}

	if (dc->connect_data != NULL) {
		purple_proxy_connect_cancel(dc->connect_data);
	}

	if (dc->listenfd != -1) {
		purple_network_remove_port_mapping(dc->listenfd);
		close(dc->listenfd);
	}

	if (dc->listenfd_handle != 0) {
		purple_timeout_remove(dc->listenfd_handle);
	}

	if (dc->connect_timeout_handle != 0) {
		purple_timeout_remove(dc->connect_timeout_handle);
	}

	if (dc->fd != -1) {
		close(dc->fd);
	}

	if (dc->send_handle != 0) {
		purple_input_remove(dc->send_handle);
	}

	if (dc->recv_handle != 0) {
		purple_input_remove(dc->recv_handle);
	}

	g_free(dc->in_buffer);

	if (dc->out_queue != NULL) {
		while (!g_queue_is_empty(dc->out_queue))
			msn_dc_destroy_packet( g_queue_pop_head(dc->out_queue) );

		g_queue_free(dc->out_queue);
	}

	g_free(dc->ext_ip);

	if (dc->timeout_handle != 0) {
		purple_timeout_remove(dc->timeout_handle);
	}

	g_free(dc);
}

/*
void
msn_dc_ref(MsnDirectConn *dc)
{
	g_return_if_fail(dc != NULL);

	dc->num_calls++;
}

void
msn_dc_unref(MsnDirectConn *dc)
{
	g_return_if_fail(dc != NULL);


	if (dc->num_calls > 0) {
		dc->num_calls--;
	}
}
*/

void
msn_dc_send_invite(MsnDirectConn *dc)
{
	MsnSlpCall    *slpcall;
	MsnSlpMessage *msg;
	gchar *header;

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "msn_dc_send_invite %p\n", dc);

	g_return_if_fail(dc != NULL);

	slpcall = dc->slpcall;
	g_return_if_fail(slpcall != NULL);

	header = g_strdup_printf(
		"INVITE MSNMSGR:%s MSNSLP/1.0",
		slpcall->slplink->remote_user
	);

	msg = msn_slpmsg_sip_new(
		slpcall,
		0,
		header,
		slpcall->branch,
		"application/x-msnmsgr-transrespbody",
		dc->msg_body
	);
	msg->info = "DC INVITE";
	msg->text_body = TRUE;
	g_free(header);
	g_free(dc->msg_body);
	dc->msg_body = NULL;

	msn_slplink_queue_slpmsg(slpcall->slplink, msg);
}

void
msn_dc_send_ok(MsnDirectConn *dc)
{
	if (purple_debug_is_verbose())
		purple_debug_info("msn", "msn_dc_send_ok %p\n", dc);

	g_return_if_fail(dc != NULL);

	msn_slp_send_ok(dc->slpcall, dc->slpcall->branch,
		"application/x-msnmsgr-transrespbody", dc->msg_body);
	g_free(dc->msg_body);
	dc->msg_body = NULL;

	msn_slplink_send_slpmsg(dc->slpcall->slplink, dc->prev_ack);
	msn_slpmsg_destroy(dc->prev_ack);
	dc->prev_ack = NULL;
	msn_slplink_send_queued_slpmsgs(dc->slpcall->slplink);
}

static void
msn_dc_fallback_to_p2p(MsnDirectConn *dc)
{
	MsnSlpCall *slpcall;
	PurpleXfer *xfer;

	purple_debug_info("msn", "msn_dc_try_fallback_to_p2p %p\n", dc);

	g_return_if_fail(dc != NULL);

	slpcall = dc->slpcall;
	g_return_if_fail(slpcall != NULL);

	xfer = slpcall->xfer;
	g_return_if_fail(xfer != NULL);

	msn_dc_destroy(dc);

	msn_slpcall_session_init(slpcall);

	/*
	switch (purple_xfer_get_status(xfer)) {
	case PURPLE_XFER_STATUS_NOT_STARTED:
	case PURPLE_XFER_STATUS_ACCEPTED:
		msn_slpcall_session_init(slpcall);
		break;

	case PURPLE_XFER_STATUS_STARTED:
		slpcall->session_init_cb = NULL;
		slpcall->end_cb = NULL;
		slpcall->progress_cb = NULL;
		slpcall->cb = NULL;

		if (fail_local)
			purple_xfer_cancel_local(xfer);
		else
			purple_xfer_cancel_remote(xfer);
		break;

	default:
		slpcall->session_init_cb = NULL;
		slpcall->end_cb = NULL;
		slpcall->progress_cb = NULL;
		slpcall->cb = NULL;

		if (fail_local)
			purple_xfer_cancel_local(xfer);
		else
			purple_xfer_cancel_remote(xfer);

		break;
	}
	*/
}

static void
msn_dc_parse_binary_header(MsnDirectConn *dc)
{
	MsnSlpHeader *h;
	MsnDcContext *context;

	g_return_if_fail(dc != NULL);

	h = &dc->header;
	/* Skip packet size */
	context = (MsnDcContext *)(dc->in_buffer + 4);

	h->session_id = GUINT32_FROM_LE(context->session_id);
	h->id = GUINT32_FROM_LE(context->seq_id);
	h->offset = GUINT64_FROM_LE(context->offset);
	h->total_size = GUINT64_FROM_LE(context->total_size);
	h->length = GUINT32_FROM_LE(context->length);
	h->flags = GUINT32_FROM_LE(context->flags);
	h->ack_id = GUINT32_FROM_LE(context->ack_id);
	h->ack_sub_id = GUINT32_FROM_LE(context->ack_uid);
	h->ack_size = GUINT64_FROM_LE(context->ack_size);
}

static const gchar *
msn_dc_serialize_binary_header(MsnDirectConn *dc) {
	MsnSlpHeader *h;
	static MsnDcContext bin_header;

	g_return_val_if_fail(dc != NULL, NULL);

	h = &dc->header;

	bin_header.session_id = GUINT32_TO_LE(h->session_id);
	bin_header.seq_id = GUINT32_TO_LE(h->id);
	bin_header.offset = GUINT64_TO_LE(h->offset);
	bin_header.total_size = GUINT64_TO_LE(h->total_size);
	bin_header.length = GUINT32_TO_LE(h->length);
	bin_header.flags = GUINT32_TO_LE(h->flags);
	bin_header.ack_id = GUINT32_TO_LE(h->ack_id);
	bin_header.ack_uid = GUINT32_TO_LE(h->ack_sub_id);
	bin_header.ack_size = GUINT64_TO_LE(h->ack_size);

	return (const gchar *)&bin_header;
}

/*
static void
msn_dc_send_bye(MsnDirectConn *dc)
{
	MsnSlpLink *slplink;
	PurpleAccount *account;
	char *body;
	int body_len;

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "msn_dc_send_bye %p\n", dc);

	g_return_if_fail(dc != NULL);
	g_return_if_fail(dc->slpcall != NULL);

	slplink = dc->slpcall->slplink;
	account = slplink->session->account;

	dc->header.session_id = 0;
	dc->header.id = dc->slpcall->slplink->slp_seq_id++;
	dc->header.offset = 0;

	body = g_strdup_printf(
		"BYE MSNMSGR:%s MSNSLP/1.0\r\n"
		"To: <msnmsgr:%s>\r\n"
		"From: <msnmsgr:%s>\r\n"
		"Via: MSNSLP/1.0/TLP ;branch={%s}\r\n"
		"CSeq: 0\r\n"
		"Call-ID: {%s}\r\n"
		"Max-Forwards: 0\r\n"
		"Content-Type: application/x-msnmsgr-sessionclosebody\r\n"
		"Content-Length: 3\r\n"
		"\r\n\r\n",

		slplink->remote_user,
		slplink->remote_user,
		purple_account_get_username(account),
		dc->slpcall->branch,
		dc->slpcall->id
	);
	body_len = strlen(body) + 1;
	memcpy(dc->buffer, body, body_len);
	g_free(body);

	dc->header.total_size = body_len;
	dc->header.length = body_len;
	dc->header.flags = 0;
	dc->header.ack_sub_id = 0;
	dc->header.ack_size = 0;

	msn_dc_send_packet(dc);
}

static void
msn_dc_send_ack(MsnDirectConn *dc)
{
	g_return_if_fail(dc != NULL);

	dc->header.session_id = 0;
	dc->header.ack_sub_id = dc->header.ack_id;
	dc->header.ack_id = dc->header.id;
	dc->header.id = dc->slpcall->slplink->slp_seq_id++;
	dc->header.offset = 0;
	dc->header.length = 0;
	dc->header.flags = 0x02;
	dc->header.ack_size = dc->header.total_size;

	msn_dc_send_packet(dc);
}

static void
msn_dc_send_data_ack(MsnDirectConn *dc)
{
	g_return_if_fail(dc != NULL);

	dc->header.session_id = dc->slpcall->session_id;
	dc->header.ack_sub_id = dc->header.ack_id;
	dc->header.ack_id = dc->header.id;
	dc->header.id = dc->slpcall->slplink->slp_seq_id++;
	dc->header.offset = 0;
	dc->header.length = 0;
	dc->header.flags = 0x02;
	dc->header.ack_size = dc->header.total_size;

	msn_dc_send_packet(dc);
}

static void
msn_dc_xfer_send_cancel(PurpleXfer *xfer)
{
	MsnSlpCall *slpcall;
	MsnDirectConn *dc;

	purple_debug_info("msn", "msn_dc_xfer_send_cancel\n");

	g_return_if_fail(xfer != NULL);

	slpcall = xfer->data;
	g_return_if_fail(slpcall != NULL);

	dc = slpcall->dc;
	g_return_if_fail(dc != NULL);

	switch (dc->state) {
	case DC_STATE_TRANSFER:
		msn_dc_send_bye(dc);
		dc->state = DC_STATE_CANCELLED;
		break;

	default:
		msn_dc_destroy(dc);
		break;
	}
}

static void
msn_dc_xfer_recv_cancel(PurpleXfer *xfer)
{
	MsnSlpCall *slpcall;
	MsnDirectConn *dc;

	purple_debug_info("msn", "msn_dc_xfer_recv_cancel\n");

	g_return_if_fail(xfer != NULL);

	slpcall = xfer->data;
	g_return_if_fail(slpcall != NULL);

	dc = slpcall->dc;
	g_return_if_fail(dc != NULL);

	switch (dc->state) {
	case DC_STATE_TRANSFER:
		msn_dc_send_bye(dc);
		dc->state = DC_STATE_CANCELLED;
		break;

	default:
		msn_dc_destroy(dc);
		break;
	}
}
*/

static void
msn_dc_send_cb(gpointer data, gint fd, PurpleInputCondition cond)
{
	MsnDirectConn *dc = data;
	MsnDirectConnPacket *p;
	int bytes_to_send;
	int bytes_sent;

	g_return_if_fail(dc != NULL);
	g_return_if_fail(fd != -1);

	if (g_queue_is_empty(dc->out_queue)) {
		if (dc->send_handle != 0) {
			purple_input_remove(dc->send_handle);
			dc->send_handle = 0;
		}
		return;
	}

	p = g_queue_peek_head(dc->out_queue);

	if (dc->msg_pos < 0) {
		/* First we send the length of the packet */
		guint32 len = GUINT32_TO_LE(p->length);
		bytes_sent = send(fd, &len, 4, 0);
		if (bytes_sent < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
				return;

			purple_debug_warning("msn", "msn_dc_send_cb: send error\n");
			msn_dc_destroy(dc);
			return;
		}
		dc->msg_pos = 0;
	}

	bytes_to_send = p->length - dc->msg_pos;
	bytes_sent = send(fd, p->data, bytes_to_send, 0);
	if (bytes_sent < 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return;

		purple_debug_warning("msn", "msn_dc_send_cb: send error\n");
		msn_dc_destroy(dc);
		return;
	}

	dc->progress = TRUE;

	dc->msg_pos += bytes_sent;
	if (dc->msg_pos == p->length) {
		if (p->sent_cb != NULL)
			p->sent_cb(p);

		g_queue_pop_head(dc->out_queue);
		msn_dc_destroy_packet(p);

		dc->msg_pos = -1;
	}
}

static void
msn_dc_enqueue_packet(MsnDirectConn *dc, MsnDirectConnPacket *p)
{
	gboolean was_empty;

	was_empty = g_queue_is_empty(dc->out_queue);
	g_queue_push_tail(dc->out_queue, p);

	if (was_empty && dc->send_handle == 0) {
		dc->send_handle = purple_input_add(dc->fd, PURPLE_INPUT_WRITE, msn_dc_send_cb, dc);
		msn_dc_send_cb(dc, dc->fd, PURPLE_INPUT_WRITE);
	}
}

static void
msn_dc_send_foo(MsnDirectConn *dc)
{
	MsnDirectConnPacket	*p;

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "msn_dc_send_foo %p\n", dc);

	p = msn_dc_new_packet(4);

	memcpy(p->data, "foo\0", 4);

	msn_dc_enqueue_packet(dc, p);
}

static void
msn_dc_send_handshake(MsnDirectConn *dc)
{
	MsnDirectConnPacket *p;
	const gchar *h;

	p = msn_dc_new_packet(DC_PACKET_HEADER_SIZE);

	dc->header.session_id = 0;
	dc->header.id = dc->slpcall->slplink->slp_seq_id++;
	dc->header.offset = 0;
	dc->header.total_size = 0;
	dc->header.length = 0;
	dc->header.flags = 0x100;

	h = msn_dc_serialize_binary_header(dc);
	memcpy(p->data, h, DC_PACKET_HEADER_SIZE);
	memcpy(p->data + offsetof(MsnDcContext, ack_id), dc->nonce, 16);

	msn_dc_enqueue_packet(dc, p);
}

static void
msn_dc_send_handshake_reply(MsnDirectConn *dc)
{
	MsnDirectConnPacket *p;
	const gchar *h;

	p = msn_dc_new_packet(DC_PACKET_HEADER_SIZE);

	dc->header.id = dc->slpcall->slplink->slp_seq_id++;
	dc->header.length = 0;

	h = msn_dc_serialize_binary_header(dc);
	memcpy(p->data, h, DC_PACKET_HEADER_SIZE);
	memcpy(p->data + offsetof(MsnDcContext, ack_id), dc->nonce, 16);

	msn_dc_enqueue_packet(dc, p);
}

static gboolean
msn_dc_verify_handshake(MsnDirectConn *dc, guint32 packet_length)
{
	guchar nonce[16];
	gchar  nonce_hash[37];

	if (packet_length != DC_PACKET_HEADER_SIZE)
		return FALSE;

	memcpy(nonce, dc->in_buffer + 4 + offsetof(MsnDcContext, ack_id), 16);

	msn_dc_calculate_nonce_hash(dc->nonce_type, nonce, nonce_hash);

	if (g_str_equal(dc->remote_nonce, nonce_hash)) {
		purple_debug_info("msn",
				"Received nonce %s from buddy request "
				"and calculated nonce %s from DC attempt. "
				"Nonces match, allowing direct connection\n",
				dc->remote_nonce, nonce_hash);
		return TRUE;
	} else {
		purple_debug_warning("msn",
				"Received nonce %s from buddy request "
				"and calculated nonce %s from DC attempt. "
				"Nonces don't match, ignoring direct connection\n",
				dc->remote_nonce, nonce_hash);
		return FALSE;
	}
}

static void
msn_dc_send_packet_cb(MsnDirectConnPacket *p)
{
	if (p->msg != NULL && p->msg->ack_cb != NULL)
		p->msg->ack_cb(p->msg, p->msg->ack_data);
}

void
msn_dc_enqueue_msg(MsnDirectConn *dc, MsnMessage *msg)
{
	MsnDirectConnPacket *p;
	guint32 length;

	length = msg->body_len + DC_PACKET_HEADER_SIZE;
	p = msn_dc_new_packet(length);

	memcpy(p->data, &msg->msnslp_header, DC_PACKET_HEADER_SIZE);
	memcpy(p->data + DC_PACKET_HEADER_SIZE, msg->body, msg->body_len);

	p->sent_cb = msn_dc_send_packet_cb;
	p->msg = msn_message_ref(msg);

	msn_dc_enqueue_packet(dc, p);
}

static int
msn_dc_process_packet(MsnDirectConn *dc, guint32 packet_length)
{
	g_return_val_if_fail(dc != NULL, DC_PROCESS_ERROR);

	switch (dc->state) {
	case DC_STATE_CLOSED:
		break;

	case DC_STATE_FOO:
		/* FOO message is always 4 bytes long */
		if (packet_length != 4 || memcmp(dc->in_buffer, "\4\0\0\0foo", 8) != 0)
			return DC_PROCESS_FALLBACK;

		dc->state = DC_STATE_HANDSHAKE;
		break;

	case DC_STATE_HANDSHAKE:
		if (!msn_dc_verify_handshake(dc, packet_length))
			return DC_PROCESS_FALLBACK;

		msn_dc_send_handshake_reply(dc);
		dc->state = DC_STATE_ESTABLISHED;

		msn_slpcall_session_init(dc->slpcall);
		dc->slpcall = NULL;
		break;

	case DC_STATE_HANDSHAKE_REPLY:
		if (!msn_dc_verify_handshake(dc, packet_length))
			return DC_PROCESS_FALLBACK;

		dc->state = DC_STATE_ESTABLISHED;

		msn_slpcall_session_init(dc->slpcall);
		dc->slpcall = NULL;
		break;

	case DC_STATE_ESTABLISHED:
		msn_slplink_process_msg(
			dc->slplink,
			&dc->header,
			dc->in_buffer + 4 + DC_PACKET_HEADER_SIZE,
			dc->header.length
		);

		/*
		if (dc->num_calls == 0) {
			msn_dc_destroy(dc);

			return DC_PROCESS_CLOSE;
		}
		*/
		break;
#if 0
		{
		guint64 file_size;
		int bytes_written;
		PurpleXfer *xfer;
		MsnSlpHeader *h = &dc->header;

		if (packet_length < DC_PACKET_HEADER_SIZE)
			return DC_TRANSFER_FALLBACK;

		/*
		 * TODO: MSN Messenger 7.0 sends BYE with flags 0x0000000 so we'll get rid of
		 * 0x1000000 bit but file data is always sent with flags 0x1000030 in both
		 * MSN Messenger and Live.*/
		switch (h->flags) {
		case 0x0000000:
		case 0x1000000:
			msn_dc_send_ack(dc);
			if (strncmp(dc->buffer, "BYE", 3) == 0) {
				/* Remote side cancelled the transfer. */
				purple_xfer_cancel_remote(dc->slpcall->xfer);
				return DC_TRANSFER_CANCELLED;
			}
			break;

		case 0x1000030:
			/* File data */
			xfer = dc->slpcall->xfer;
			file_size = purple_xfer_get_size(xfer);

			/* Packet sanity checks */
			if (	h->session_id != dc->slpcall->session_id ||
				h->offset >= file_size ||
				h->total_size != file_size ||
				h->length != packet_length - DC_PACKET_HEADER_SIZE ||
				h->offset + h->length > file_size) {

				purple_debug_warning("msn", "msn_dc_recv_process_packet_cb: packet range check error!\n");
				purple_xfer_cancel_local(dc->slpcall->xfer);
				return DC_TRANSFER_CANCELLED;
			}

			bytes_written = fwrite(dc->buffer, 1, h->length, xfer->dest_fp);
			if (bytes_written != h->length) {
				purple_debug_warning("msn", "msn_dc_recv_process_packet_cb: cannot write whole packet to file!\n");
				purple_xfer_cancel_local(dc->slpcall->xfer);
				return DC_TRANSFER_CANCELLED;
			}

			xfer->bytes_sent = (h->offset + h->length);
			xfer->bytes_remaining = h->total_size - xfer->bytes_sent;

			purple_xfer_update_progress(xfer);

			if (xfer->bytes_remaining == 0) {
				/* ACK only the last data packet */
				msn_dc_send_data_ack(dc);
				purple_xfer_set_completed(xfer, TRUE);
				dc->state = DC_STATE_BYE;
			}
			break;
		default:
			/*
			 * TODO: Packet with unknown flags. Should we ACK these?
			 */
			msn_dc_send_ack(dc);

			purple_debug_warning(
				"msn",
				"msn_dc_recv_process_packet_cb: received packet with unknown flags: 0x%08x\n",
				dc->header.flags
			);
		}
		break;
		}

	case DC_STATE_BYE:
		/* TODO: Check! */
		switch (dc->header.flags) {
		case 0x0000000:
		case 0x1000000:
			msn_dc_send_ack(dc);
			if (strncmp(dc->buffer, "BYE", 3) == 0) {
				dc->state = DC_STATE_COMPLETED;
				return DC_TRANSFER_COMPLETED;
			}
			break;

		default:
			/*
			 * TODO: Packet with unknown flags. Should we ACK these?
			 */
			msn_dc_send_ack(dc);
			purple_debug_warning(
				"msn",
				"msn_dc_recv_process_packet_cb: received packet with unknown flags: 0x%08x\n",
				dc->header.flags
			);
		}
		break;
#endif
	}

	return DC_PROCESS_OK;
}

static void
msn_dc_recv_cb(gpointer data, gint fd, PurpleInputCondition cond)
{
	MsnDirectConn *dc;
	int free_buf_space;
	int bytes_received;
	guint32 packet_length;

	g_return_if_fail(data != NULL);
	g_return_if_fail(fd != -1);

	dc = data;
	free_buf_space = dc->in_size - dc->in_pos;

	bytes_received = recv(fd, dc->in_buffer + dc->in_pos, free_buf_space, 0);
	if (bytes_received < 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return;

		purple_debug_warning("msn", "msn_dc_recv_cb: recv error\n");

		if(dc->state != DC_STATE_ESTABLISHED)
			msn_dc_fallback_to_p2p(dc);
		else
			msn_dc_destroy(dc);
		return;

	} else if (bytes_received == 0) {
		/* EOF. Remote side closed connection. */
		purple_debug_info("msn", "msn_dc_recv_cb: recv EOF\n");

		if(dc->state != DC_STATE_ESTABLISHED)
			msn_dc_fallback_to_p2p(dc);
		else
			msn_dc_destroy(dc);
		return;
	}

	dc->progress = TRUE;

	dc->in_pos += bytes_received;

	/* Wait for packet length */
	while (dc->in_pos >= 4) {
		packet_length = *((guint32*)dc->in_buffer);
		packet_length = GUINT32_FROM_LE(packet_length);

		if (packet_length > DC_MAX_PACKET_SIZE) {
			/* Oversized packet */
			purple_debug_warning("msn", "msn_dc_recv_cb: oversized packet received\n");
			return;
		}

		/* Wait for the whole packet to arrive */
		if (dc->in_pos < 4 + packet_length)
			return;

		if (dc->state != DC_STATE_FOO) {
			msn_dc_parse_binary_header(dc);
		}

		switch (msn_dc_process_packet(dc, packet_length)) {
		case DC_PROCESS_CLOSE:
			return;

		case DC_PROCESS_FALLBACK:
			purple_debug_warning("msn", "msn_dc_recv_cb: packet processing error, fall back to p2p\n");
			msn_dc_fallback_to_p2p(dc);
			return;

		}

		if (dc->in_pos > packet_length + 4) {
			memcpy(dc->in_buffer, dc->in_buffer + 4 + packet_length, dc->in_pos - packet_length - 4);
		}

		dc->in_pos -= packet_length + 4;
	}
}

#if 0
static gboolean
msn_dc_send_next_packet(MsnDirectConn *dc)
{
	MsnSlpMessage *msg;

	if(g_queue_is_empty(dc->out_queue))
		return TRUE;

	msg = g_queue_peek_head(dc->out_queue);
	msn_slplink_send_msgpart(dc->slplink, msg);



	PurpleXfer *xfer;
	int bytes_read;

	g_return_val_if_fail(dc != NULL, FALSE);
	g_return_val_if_fail(dc->slpcall != NULL, FALSE);

	xfer = dc->slpcall->xfer;

	bytes_read = fread(dc->buffer, 1, DC_MAX_BODY_SIZE, xfer->dest_fp);

	if (bytes_read > 0) {
		dc->header.session_id = dc->slpcall->session_id;
		/* Only increment seq. ID before sending BYE */
		dc->header.id = dc->slpcall->slplink->slp_seq_id;
		dc->header.offset = xfer->bytes_sent;
		dc->header.total_size = xfer->size;
		dc->header.length = bytes_read;
		dc->header.flags = 0x1000030;
		dc->header.ack_id = rand() % G_MAXUINT32;
		dc->header.ack_sub_id = 0;
		dc->header.ack_size = 0;

		msn_dc_send_packet(dc);

		xfer->bytes_sent += bytes_read;
		xfer->bytes_remaining -= bytes_read;
		purple_xfer_update_progress(xfer);

		if (xfer->bytes_remaining == 0) {
			purple_xfer_set_completed(xfer, TRUE);

			/* Increment seq. ID for the next BYE message */
			dc->slpcall->slplink->slp_seq_id++;
			dc->state = DC_STATE_DATA_ACK;
		}

	} else {
		/* File read error */
		purple_xfer_cancel_local(xfer);
		return FALSE;
	}

	return TRUE;
}

static int
msn_dc_send_process_packet_cb(MsnDirectConn *dc, guint32 packet_length)
{
	g_return_val_if_fail(dc != NULL, DC_TRANSFER_CANCELLED);

	switch (dc->state) {
	case DC_STATE_FOO: {
		if (packet_length != 4)
			return DC_TRANSFER_FALLBACK;

		if (memcmp(dc->in_buffer, "\4\0\0\0foo", 8) != 0)
			return DC_TRANSFER_FALLBACK;

		dc->state = DC_STATE_HANDSHAKE;
		break;
		}

	case DC_STATE_HANDSHAKE: {
		if (packet_length != DC_PACKET_HEADER_SIZE)
			return DC_TRANSFER_FALLBACK;

		/* TODO: Check! */
		msn_dc_send_handshake_reply(dc);
		dc->state = DC_STATE_TRANSFER;

		purple_xfer_set_request_denied_fnc(dc->slpcall->xfer, msn_dc_xfer_send_cancel);
		purple_xfer_set_cancel_send_fnc(dc->slpcall->xfer, msn_dc_xfer_send_cancel);
		purple_xfer_set_end_fnc(dc->slpcall->xfer, msn_dc_xfer_end);
		purple_xfer_start(dc->slpcall->xfer, -1, NULL, 0);
		break;
		}

	case DC_STATE_HANDSHAKE_REPLY:
		/* TODO: Check! */
		dc->state = DC_STATE_TRANSFER;
		break;

	case DC_STATE_TRANSFER: {
		switch (dc->header.flags) {
		case 0x0000000:
		case 0x1000000:
			msn_dc_send_ack(dc);
			if (strncmp(dc->buffer, "BYE", 3) == 0) {
				/* Remote side cancelled the transfer. */
				purple_xfer_cancel_remote(dc->slpcall->xfer);
				return DC_TRANSFER_CANCELLED;
			}
			break;
		}
		break;
		}

	case DC_STATE_DATA_ACK: {
		/* TODO: Check! */
		msn_dc_send_bye(dc);
		dc->state = DC_STATE_BYE_ACK;
		break;
		}

	case DC_STATE_BYE_ACK:
		/* TODO: Check! */
		dc->state = DC_STATE_COMPLETED;
		return DC_TRANSFER_COMPLETED;
	}

	return DC_TRANSFER_OK;
}
#endif

static gboolean
msn_dc_timeout(gpointer data)
{
	MsnDirectConn *dc = data;

	g_return_val_if_fail(dc != NULL, FALSE);

	if (dc->progress)
		dc->progress = FALSE;
	else
		msn_dc_destroy(dc);

	return TRUE;
}

static void
msn_dc_init(MsnDirectConn *dc)
{
	g_return_if_fail(dc != NULL);

	dc->in_size = DC_MAX_PACKET_SIZE + 4;
	dc->in_pos = 0;
	dc->in_buffer = g_malloc(dc->in_size);

	dc->recv_handle = purple_input_add(dc->fd, PURPLE_INPUT_READ, msn_dc_recv_cb, dc);
	dc->send_handle = purple_input_add(dc->fd, PURPLE_INPUT_WRITE, msn_dc_send_cb, dc);

	dc->timeout_handle = purple_timeout_add_seconds(DC_TIMEOUT, msn_dc_timeout, dc);
}

void
msn_dc_connected_to_peer_cb(gpointer data, gint fd, const gchar *error_msg)
{
	MsnDirectConn *dc = data;

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "msn_dc_connected_to_peer_cb %p\n", dc);

	g_return_if_fail(dc != NULL);

	dc->connect_data = NULL;
	purple_timeout_remove(dc->connect_timeout_handle);
	dc->connect_timeout_handle = 0;

	dc->fd = fd;
	if (dc->fd != -1) {
		msn_dc_init(dc);
		msn_dc_send_foo(dc);
		msn_dc_send_handshake(dc);
		dc->state = DC_STATE_HANDSHAKE_REPLY;
	}
}

/*
 * This callback will be called when we're the server
 * and nobody has connected us in DC_CONNECT_TIMEOUT seconds
 */
static gboolean
msn_dc_incoming_connection_timeout_cb(gpointer data) {
	MsnDirectConn *dc = data;
	MsnSlpCall *slpcall = dc->slpcall;

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "msn_dc_incoming_connection_timeout_cb %p\n", dc);

	g_return_val_if_fail(dc != NULL, FALSE);

	slpcall = dc->slpcall;
	g_return_val_if_fail(slpcall != NULL, FALSE);

	if (dc->listen_data != NULL) {
		purple_network_listen_cancel(dc->listen_data);
		dc->listen_data = NULL;
	}

	if (dc->listenfd_handle != 0) {
		purple_timeout_remove(dc->listenfd_handle);
		dc->listenfd_handle = 0;
	}

	if (dc->listenfd != -1) {
		purple_network_remove_port_mapping(dc->listenfd);
		close(dc->listenfd);
		dc->listenfd = -1;
	}

	msn_dc_destroy(dc);
	/* Start p2p file transfer */
	msn_slpcall_session_init(slpcall);

	return FALSE;
}

/*
 * This callback will be called when we're unable to connect to
 * the remote host in DC_CONNECT_TIMEOUT seconds.
 */
gboolean
msn_dc_outgoing_connection_timeout_cb(gpointer data)
{
	MsnDirectConn *dc = data;

	purple_debug_info("msn", "msn_dc_outgoing_connection_timeout_cb %p\n", dc);

	g_return_val_if_fail(dc != NULL, FALSE);

	if (dc->connect_timeout_handle != 0) {
		purple_timeout_remove(dc->connect_timeout_handle);
		dc->connect_timeout_handle = 0;
	}

	if (dc->connect_data != NULL) {
		purple_proxy_connect_cancel(dc->connect_data);
		dc->connect_data = NULL;
	}

	if (dc->ext_ip && dc->ext_port) {
		/* Try external IP/port if available. */
		dc->connect_data = purple_proxy_connect(
			NULL,
			dc->slpcall->slplink->session->account,
			dc->ext_ip,
			dc->ext_port,
			msn_dc_connected_to_peer_cb,
			dc
		);

		g_free(dc->ext_ip);
		dc->ext_ip = NULL;

		if (dc->connect_data) {
			dc->connect_timeout_handle = purple_timeout_add_seconds(
				DC_CONNECT_TIMEOUT,
				msn_dc_outgoing_connection_timeout_cb,
				dc
			);
		}

	} else {
		/*
		 * Both internal and external connection attempts are failed.
		 * Fall back to p2p transfer.
		 */
		MsnSlpCall	*slpcall = dc->slpcall;

		msn_dc_destroy(dc);
		/* Start p2p file transfer */
		msn_slpcall_session_init(slpcall);
	}

	return FALSE;
}

/*
 * This callback will be called when we're the server
 * and somebody has connected to us in DC_CONNECT_TIMEOUT seconds.
 */
static void
msn_dc_incoming_connection_cb(gpointer data, gint listenfd, PurpleInputCondition cond)
{
	MsnDirectConn *dc = data;

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "msn_dc_incoming_connection_cb %p\n", dc);

	g_return_if_fail(dc != NULL);

	if (dc->connect_timeout_handle != 0) {
		purple_timeout_remove(dc->connect_timeout_handle);
		dc->connect_timeout_handle = 0;
	}

	if (dc->listenfd_handle != 0) {
		purple_input_remove(dc->listenfd_handle);
		dc->listenfd_handle = 0;
	}

	dc->fd = accept(listenfd, NULL, 0);

	purple_network_remove_port_mapping(dc->listenfd);
	close(dc->listenfd);
	dc->listenfd = -1;

	if (dc->fd != -1) {
		msn_dc_init(dc);
		dc->state = DC_STATE_FOO;
	}
}

void
msn_dc_listen_socket_created_cb(int listenfd, gpointer data)
{
	MsnDirectConn *dc = data;

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "msn_dc_listen_socket_created_cb %p\n", dc);

	g_return_if_fail(dc != NULL);

	dc->listen_data = NULL;

	if (listenfd != -1) {
		const char *ext_ip;
		const char *int_ip;
		int port;

		ext_ip = purple_network_get_my_ip(listenfd);
		int_ip = purple_network_get_local_system_ip(listenfd);
		port = purple_network_get_port_from_fd(listenfd);

		dc->listenfd = listenfd;
		dc->listenfd_handle = purple_input_add(
			listenfd,
			PURPLE_INPUT_READ,
			msn_dc_incoming_connection_cb,
			dc
		);
		dc->connect_timeout_handle = purple_timeout_add_seconds(
			DC_CONNECT_TIMEOUT * 2, /* Internal + external connection attempts */
			msn_dc_incoming_connection_timeout_cb,
			dc
		);

		if (strcmp(int_ip, ext_ip) != 0) {
			dc->msg_body = g_strdup_printf(
				"Bridge: TCPv1\r\n"
				"Listening: true\r\n"
				"%sNonce: {%s}\r\n"
				"IPv4External-Addrs: %s\r\n"
				"IPv4External-Port: %d\r\n"
				"IPv4Internal-Addrs: %s\r\n"
				"IPv4Internal-Port: %d\r\n"
				"\r\n",

				dc->nonce_type != DC_NONCE_PLAIN ? "Hashed-" : "",
				dc->nonce_hash,
				ext_ip,
				port,
				int_ip,
				port
			);

		} else {
			dc->msg_body = g_strdup_printf(
				"Bridge: TCPv1\r\n"
				"Listening: true\r\n"
				"%sNonce: {%s}\r\n"
				"IPv4External-Addrs: %s\r\n"
				"IPv4External-Port: %d\r\n"
				"\r\n",

				dc->nonce_type != DC_NONCE_PLAIN ? "Hashed-" : "",
				dc->nonce_hash,
				ext_ip,
				port
			);
		}

		if (dc->slpcall->wait_for_socket) {
			if (dc->send_connection_info_msg_cb != NULL)
				dc->send_connection_info_msg_cb(dc);

			dc->slpcall->wait_for_socket = FALSE;
		}
	}
}

