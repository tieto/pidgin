/**
 * @file slplink.c MSNSLP Link support
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
#include "slplink.h"

#include "switchboard.h"
#include "slp.h"

void msn_slplink_send_msgpart(MsnSlpLink *slplink, MsnSlpMessage *slpmsg);

#ifdef MSN_DEBUG_SLP_FILES
static int m_sc = 0;
static int m_rc = 0;

static void
debug_msg_to_file(MsnMessage *msg, gboolean send)
{
	char *tmp;
	char *dir;
	char *pload;
	FILE *tf;
	int c;
	gsize pload_size;

	dir = send ? "send" : "recv";
	c = send ? m_sc++ : m_rc++;
	tmp = g_strdup_printf("%s/msntest/%s/%03d", g_get_home_dir(), dir, c);
	tf = g_fopen(tmp, "wb");
	if (tf == NULL)
	{
		gaim_debug_error("msn", "could not open debug file");
		return;
	}
	pload = msn_message_gen_payload(msg, &pload_size);
	fwrite(pload, 1, pload_size, tf);
	fclose(tf);
	g_free(tmp);
}
#endif

/**************************************************************************
 * Main
 **************************************************************************/

MsnSlpLink *
msn_slplink_new(MsnSession *session, const char *username)
{
	MsnSlpLink *slplink;

	g_return_val_if_fail(session != NULL, NULL);

	slplink = g_new0(MsnSlpLink, 1);

#ifdef MSN_DEBUG_SLPLINK
	gaim_debug_info("msn", "slplink_new: slplink(%p)\n", slplink);
#endif

	slplink->session = session;
	slplink->slp_seq_id = rand() % 0xFFFFFF00 + 4;

	slplink->local_user = g_strdup(msn_user_get_passport(session->user));
	slplink->remote_user = g_strdup(username);

	slplink->slp_msg_queue = g_queue_new();

	session->slplinks =
		g_list_append(session->slplinks, slplink);

	return slplink;
}

void
msn_slplink_destroy(MsnSlpLink *slplink)
{
	MsnSession *session;

#ifdef MSN_DEBUG_SLPLINK
	gaim_debug_info("msn", "slplink_destroy: slplink(%p)\n", slplink);
#endif

	g_return_if_fail(slplink != NULL);

	if (slplink->swboard != NULL)
		slplink->swboard->slplink = NULL;

	session = slplink->session;

	if (slplink->local_user != NULL)
		g_free(slplink->local_user);

	if (slplink->remote_user != NULL)
		g_free(slplink->remote_user);

	if (slplink->directconn != NULL)
		msn_directconn_destroy(slplink->directconn);

	while (slplink->slp_calls != NULL)
		msn_slp_call_destroy(slplink->slp_calls->data);

	session->slplinks =
		g_list_remove(session->slplinks, slplink);

	g_free(slplink);
}

MsnSlpLink *
msn_session_find_slplink(MsnSession *session, const char *who)
{
	GList *l;

	for (l = session->slplinks; l != NULL; l = l->next)
	{
		MsnSlpLink *slplink;

		slplink = l->data;

		if (!strcmp(slplink->remote_user, who))
			return slplink;
	}

	return NULL;
}

MsnSlpLink *
msn_session_get_slplink(MsnSession *session, const char *username)
{
	MsnSlpLink *slplink;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(username != NULL, NULL);

	slplink = msn_session_find_slplink(session, username);

	if (slplink == NULL)
		slplink = msn_slplink_new(session, username);

	return slplink;
}

MsnSlpSession *
msn_slplink_find_slp_session(MsnSlpLink *slplink, long session_id)
{
	GList *l;
	MsnSlpSession *slpsession;

	for (l = slplink->slp_sessions; l != NULL; l = l->next)
	{
		slpsession = l->data;

		if (slpsession->id == session_id)
			return slpsession;
	}

	return NULL;
}

void
msn_slplink_add_slpcall(MsnSlpLink *slplink, MsnSlpCall *slpcall)
{
	if (slplink->swboard != NULL)
		slplink->swboard->flag |= MSN_SB_FLAG_FT;

	slplink->slp_calls = g_list_append(slplink->slp_calls, slpcall);
}

void
msn_slplink_remove_slpcall(MsnSlpLink *slplink, MsnSlpCall *slpcall)
{
	slplink->slp_calls = g_list_remove(slplink->slp_calls, slpcall);

	/* The slplink has no slpcalls in it. If no one is using it, we might
	 * destroy the switchboard, but we should be careful not to use the slplink
	 * again. */
	if (slplink->slp_calls == NULL)
	{
		if (slplink->swboard != NULL)
		{
			if (msn_switchboard_release(slplink->swboard, MSN_SB_FLAG_FT))
				/* I'm not sure this is the best thing to do, but it's better
				 * than nothing. */
				slpcall->slplink = NULL;
		}
	}
}

MsnSlpCall *
msn_slplink_find_slp_call(MsnSlpLink *slplink, const char *id)
{
	GList *l;
	MsnSlpCall *slpcall;

	if (!id)
		return NULL;

	for (l = slplink->slp_calls; l != NULL; l = l->next)
	{
		slpcall = l->data;

		if (slpcall->id && !strcmp(slpcall->id, id))
			return slpcall;
	}

	return NULL;
}

MsnSlpCall *
msn_slplink_find_slp_call_with_session_id(MsnSlpLink *slplink, long id)
{
	GList *l;
	MsnSlpCall *slpcall;

	for (l = slplink->slp_calls; l != NULL; l = l->next)
	{
		slpcall = l->data;

		if (slpcall->session_id == id)
			return slpcall;
	}

	return NULL;
}

void
msn_slplink_send_msg(MsnSlpLink *slplink, MsnMessage *msg)
{
	if (slplink->directconn != NULL)
	{
		msn_directconn_send_msg(slplink->directconn, msg);
	}
	else
	{
		if (slplink->swboard == NULL)
		{
			slplink->swboard = msn_session_get_swboard(slplink->session,
													   slplink->remote_user, MSN_SB_FLAG_FT);

			if (slplink->swboard == NULL)
				return;

			/* If swboard is destroyed we will be too */
			slplink->swboard->slplink = slplink;
		}

		msn_switchboard_send_msg(slplink->swboard, msg, TRUE);
	}
}

/* We have received the message ack */
static void
msg_ack(MsnMessage *msg, void *data)
{
	MsnSlpMessage *slpmsg;
	long long real_size;

	slpmsg = data;

	real_size = (slpmsg->flags == 0x2) ? 0 : slpmsg->size;

	slpmsg->offset += msg->msnslp_header.length;

	if (slpmsg->offset < real_size)
	{
		msn_slplink_send_msgpart(slpmsg->slplink, slpmsg);
	}
	else
	{
		/* The whole message has been sent */
		if (slpmsg->flags == 0x20 || slpmsg->flags == 0x1000030)
		{
			if (slpmsg->slpcall != NULL)
			{
				if (slpmsg->slpcall->cb)
					slpmsg->slpcall->cb(slpmsg->slpcall,
						NULL, 0);
			}
		}
	}

	slpmsg->msgs = g_list_remove(slpmsg->msgs, msg);
}

/* We have received the message nak. */
static void
msg_nak(MsnMessage *msg, void *data)
{
	MsnSlpMessage *slpmsg;

	slpmsg = data;

	msn_slplink_send_msgpart(slpmsg->slplink, slpmsg);

	slpmsg->msgs = g_list_remove(slpmsg->msgs, msg);
}

void
msn_slplink_send_msgpart(MsnSlpLink *slplink, MsnSlpMessage *slpmsg)
{
	MsnMessage *msg;
	long long real_size;
	size_t len = 0;

	/* Maybe we will want to create a new msg for this slpmsg instead of
	 * reusing the same one all the time. */
	msg = slpmsg->msg;

	real_size = (slpmsg->flags == 0x2) ? 0 : slpmsg->size;

	if (slpmsg->offset < real_size)
	{
		if (slpmsg->fp)
		{
			char data[1202];
			len = fread(data, 1, sizeof(data), slpmsg->fp);
			msn_message_set_bin_data(msg, data, len);
		}
		else
		{
			len = slpmsg->size - slpmsg->offset;

			if (len > 1202)
				len = 1202;

			msn_message_set_bin_data(msg, slpmsg->buffer + slpmsg->offset, len);
		}

		msg->msnslp_header.offset = slpmsg->offset;
		msg->msnslp_header.length = len;
	}

#ifdef MSN_DEBUG_SLP
	msn_message_show_readable(msg, slpmsg->info, slpmsg->text_body);
#endif

#ifdef MSN_DEBUG_SLP_FILES
	debug_msg_to_file(msg, TRUE);
#endif

	slpmsg->msgs =
		g_list_append(slpmsg->msgs, msg);
	msn_slplink_send_msg(slplink, msg);

	if ((slpmsg->flags == 0x20 || slpmsg->flags == 0x1000030) &&
		(slpmsg->slpcall != NULL))
	{
		slpmsg->slpcall->progress = TRUE;

		if (slpmsg->slpcall->progress_cb != NULL)
		{
			slpmsg->slpcall->progress_cb(slpmsg->slpcall, slpmsg->size,
										 len, slpmsg->offset);
		}
	}

	/* slpmsg->offset += len; */
}

void
msn_slplink_release_slpmsg(MsnSlpLink *slplink, MsnSlpMessage *slpmsg)
{
	MsnMessage *msg;

	slpmsg->msg = msg = msn_message_new_msnslp();

	if (slpmsg->flags == 0x0)
	{
		msg->msnslp_header.session_id = slpmsg->session_id;
		msg->msnslp_header.ack_id = rand() % 0xFFFFFF00;
	}
	else if (slpmsg->flags == 0x2)
	{
		msg->msnslp_header.session_id = slpmsg->session_id;
		msg->msnslp_header.ack_id = slpmsg->ack_id;
		msg->msnslp_header.ack_size = slpmsg->ack_size;
		msg->msnslp_header.ack_sub_id = slpmsg->ack_sub_id;
	}
	else if (slpmsg->flags == 0x20 || slpmsg->flags == 0x1000030)
	{
		MsnSlpSession *slpsession;
		slpsession = slpmsg->slpsession;

		g_return_if_fail(slpsession != NULL);
		msg->msnslp_header.session_id = slpsession->id;
		msg->msnslp_footer.value = slpsession->app_id;
		msg->msnslp_header.ack_id = rand() % 0xFFFFFF00;
	}
	else if (slpmsg->flags == 0x100)
	{
		msg->msnslp_header.ack_id     = slpmsg->ack_id;
		msg->msnslp_header.ack_sub_id = slpmsg->ack_sub_id;
		msg->msnslp_header.ack_size   = slpmsg->ack_size;
	}

	msg->msnslp_header.id = slpmsg->id;
	msg->msnslp_header.flags = slpmsg->flags;

	msg->msnslp_header.total_size = slpmsg->size;

	msn_message_set_attr(msg, "P2P-Dest", slplink->remote_user);

	msg->ack_cb = msg_ack;
	msg->nak_cb = msg_nak;
	msg->ack_data = slpmsg;

	msn_slplink_send_msgpart(slplink, slpmsg);

	msn_message_destroy(msg);
}

void
msn_slplink_queue_slpmsg(MsnSlpLink *slplink, MsnSlpMessage *slpmsg)
{
	slpmsg->id = slplink->slp_seq_id++;

	g_queue_push_head(slplink->slp_msg_queue, slpmsg);
}

void
msn_slplink_send_slpmsg(MsnSlpLink *slplink, MsnSlpMessage *slpmsg)
{
	slpmsg->id = slplink->slp_seq_id++;

	msn_slplink_release_slpmsg(slplink, slpmsg);
}

void
msn_slplink_unleash(MsnSlpLink *slplink)
{
	MsnSlpMessage *slpmsg;

	/* Send the queued msgs in the order they came. */

	while ((slpmsg = g_queue_pop_tail(slplink->slp_msg_queue)) != NULL)
	{
		msn_slplink_release_slpmsg(slplink, slpmsg);
	}
}

void
msn_slplink_send_ack(MsnSlpLink *slplink, MsnMessage *msg)
{
	MsnSlpMessage *slpmsg;

	slpmsg = msn_slpmsg_new(slplink);

	slpmsg->session_id = msg->msnslp_header.session_id;
	slpmsg->size       = msg->msnslp_header.total_size;
	slpmsg->flags      = 0x02;
	slpmsg->ack_id     = msg->msnslp_header.id;
	slpmsg->ack_sub_id = msg->msnslp_header.ack_id;
	slpmsg->ack_size   = msg->msnslp_header.total_size;

#ifdef MSN_DEBUG_SLP
	slpmsg->info = "SLP ACK";
#endif

	msn_slplink_send_slpmsg(slplink, slpmsg);
}

static void
send_file_cb(MsnSlpSession *slpsession)
{
	MsnSlpCall *slpcall;
	MsnSlpMessage *slpmsg;

	slpcall = slpsession->slpcall;
	slpmsg = msn_slpmsg_new(slpcall->slplink);
	slpmsg->slpcall = slpcall;
	slpmsg->flags = 0x1000030;
	slpmsg->slpsession = slpsession;
#ifdef MSN_DEBUG_SLP
	slpmsg->info = "SLP FILE";
#endif
	msn_slpmsg_open_file(slpmsg, gaim_xfer_get_local_filename(slpcall->xfer));

	msn_slplink_send_slpmsg(slpcall->slplink, slpmsg);
}

void
msn_slplink_process_msg(MsnSlpLink *slplink, MsnMessage *msg)
{
	MsnSlpMessage *slpmsg;
	const char *data;
	gsize offset;
	gsize len;

#ifdef MSN_DEBUG_SLP
	msn_slpmsg_show(msg);
#endif

#ifdef MSN_DEBUG_SLP_FILES
	debug_msg_to_file(msg, FALSE);
#endif

	if (msg->msnslp_header.total_size < msg->msnslp_header.length)
	{
		gaim_debug_error("msn", "This can't be good\n");
		g_return_if_reached();
	}

	slpmsg = NULL;
	data = msn_message_get_bin_data(msg, &len);

	/*
		OVERHEAD!
		if (msg->msnslp_header.length < msg->msnslp_header.total_size)
	 */

	offset = msg->msnslp_header.offset;

	if (offset == 0)
	{
		slpmsg = msn_slpmsg_new(slplink);
		slpmsg->id = msg->msnslp_header.id;
		slpmsg->session_id = msg->msnslp_header.session_id;
		slpmsg->size = msg->msnslp_header.total_size;
		slpmsg->flags = msg->msnslp_header.flags;

		if (slpmsg->session_id)
		{
			if (slpmsg->slpcall == NULL)
				slpmsg->slpcall = msn_slplink_find_slp_call_with_session_id(slplink, slpmsg->session_id);

			if (slpmsg->slpcall != NULL)
			{
				if (slpmsg->flags == 0x20 || slpmsg->flags == 0x1000030)
				{
					GaimXfer *xfer;

					xfer = slpmsg->slpcall->xfer;

					if (xfer != NULL)
					{
						slpmsg->fp =
							g_fopen(gaim_xfer_get_local_filename(slpmsg->slpcall->xfer),
								  "wb");
					}
				}
			}
		}
		if (!slpmsg->fp && slpmsg->size)
		{
			slpmsg->buffer = g_try_malloc(slpmsg->size);
			if (slpmsg->buffer == NULL)
			{
				gaim_debug_error("msn", "Failed to allocate buffer for slpmsg\n");
				return;
			}
		}
	}
	else
	{
		slpmsg = msn_slplink_message_find(slplink, msg->msnslp_header.session_id, msg->msnslp_header.id);
	}

	if (slpmsg == NULL)
	{
		/* Probably the transfer was canceled */
		gaim_debug_error("msn", "Couldn't find slpmsg\n");
		return;
	}

	if (slpmsg->fp)
	{
		/* fseek(slpmsg->fp, offset, SEEK_SET); */
		len = fwrite(data, 1, len, slpmsg->fp);
	}
	else if (slpmsg->size)
	{
		if ((offset + len) > slpmsg->size)
		{
			gaim_debug_error("msn", "Oversized slpmsg\n");
			g_return_if_reached();
		}
		else
			memcpy(slpmsg->buffer + offset, data, len);
	}

	if ((slpmsg->flags == 0x20 || slpmsg->flags == 0x1000030) &&
		(slpmsg->slpcall != NULL))
	{
		slpmsg->slpcall->progress = TRUE;

		if (slpmsg->slpcall->progress_cb != NULL)
		{
			slpmsg->slpcall->progress_cb(slpmsg->slpcall, slpmsg->size,
										 len, offset);
		}
	}

#if 0
	if (slpmsg->buffer == NULL)
		return;
#endif

	if (msg->msnslp_header.offset + msg->msnslp_header.length
		>= msg->msnslp_header.total_size)
	{
		/* All the pieces of the slpmsg have been received */
		MsnSlpCall *slpcall;

		slpcall = msn_slp_process_msg(slplink, slpmsg);

		if (slpmsg->flags == 0x100)
		{
			MsnDirectConn *directconn;

			directconn = slplink->directconn;

			if (!directconn->acked)
				msn_directconn_send_handshake(directconn);
		}
		else if (slpmsg->flags == 0x0 || slpmsg->flags == 0x20 ||
				 slpmsg->flags == 0x1000030)
		{
			/* Release all the messages and send the ACK */

			msn_slplink_send_ack(slplink, msg);
			msn_slplink_unleash(slplink);
		}

		msn_slpmsg_destroy(slpmsg);

		if (slpcall != NULL && slpcall->wasted)
			msn_slp_call_destroy(slpcall);
	}
}

MsnSlpMessage *
msn_slplink_message_find(MsnSlpLink *slplink, long session_id, long id)
{
	GList *e;

	for (e = slplink->slp_msgs; e != NULL; e = e->next)
	{
		MsnSlpMessage *slpmsg = e->data;

		if ((slpmsg->session_id == session_id) && (slpmsg->id == id))
			return slpmsg;
	}

	return NULL;
}

typedef struct
{
	guint32 length;
	guint32 unk1;
	guint32 file_size;
	guint32 unk2;
	guint32 unk3;
} MsnContextHeader;

#define MAX_FILE_NAME_LEN 0x226

static gchar *
gen_context(const char *file_name, const char *file_path)
{
	struct stat st;
	gsize size = 0;
	MsnContextHeader header;
	gchar *u8 = NULL;
	guchar *base;
	guchar *n;
	gchar *ret;
	gunichar2 *uni = NULL;
	glong currentChar = 0;
	glong uni_len = 0;
	gsize len;

	if (g_stat(file_path, &st) == 0)
		size = st.st_size;

	if(!file_name) {
		u8 = gaim_utf8_try_convert(g_basename(file_path));
		file_name = u8;
	}

	uni = g_utf8_to_utf16(file_name, -1, NULL, &uni_len, NULL);

	if(u8) {
		g_free(u8);
		file_name = NULL;
		u8 = NULL;
	}

	len = sizeof(MsnContextHeader) + MAX_FILE_NAME_LEN + 4;

	header.length = GUINT32_TO_LE(len);
	header.unk1 = GUINT32_TO_LE(2);
	header.file_size = GUINT32_TO_LE(size);
	header.unk2 = GUINT32_TO_LE(0);
	header.unk3 = GUINT32_TO_LE(0);

	base = g_malloc(len + 1);
	n = base;

	memcpy(n, &header, sizeof(MsnContextHeader));
	n += sizeof(MsnContextHeader);

	memset(n, 0x00, MAX_FILE_NAME_LEN);
	for(currentChar = 0; currentChar < uni_len; currentChar++) {
		*((gunichar2 *)n + currentChar) = GUINT16_TO_LE(uni[currentChar]);
	}
	n += MAX_FILE_NAME_LEN;

	memset(n, 0xFF, 4);
	n += 4;

	g_free(uni);
	ret = gaim_base64_encode(base, len);
	g_free(base);
	return ret;
}

void
msn_slplink_request_ft(MsnSlpLink *slplink, GaimXfer *xfer)
{
	MsnSlpCall *slpcall;
	char *context;
	const char *fn;
	const char *fp;

	fn = gaim_xfer_get_filename(xfer);
	fp = gaim_xfer_get_local_filename(xfer);

	g_return_if_fail(slplink != NULL);
	g_return_if_fail(fp != NULL);

	slpcall = msn_slp_call_new(slplink);
	msn_slp_call_init(slpcall, MSN_SLPCALL_DC);

	slpcall->session_init_cb = send_file_cb;
	slpcall->end_cb = msn_xfer_end_cb;
	slpcall->progress_cb = msn_xfer_progress_cb;
	slpcall->cb = msn_xfer_completed_cb;
	slpcall->xfer = xfer;

	slpcall->pending = TRUE;

	gaim_xfer_set_cancel_send_fnc(xfer, msn_xfer_cancel);

	xfer->data = slpcall;

	context = gen_context(fn, fp);

	msn_slp_call_invite(slpcall, "5D3E02AB-6190-11D3-BBBB-00C04F795683", 2,
						context);

	g_free(context);
}

void
msn_slplink_request_object(MsnSlpLink *slplink,
						   const char *info,
						   MsnSlpCb cb,
						   MsnSlpEndCb end_cb,
						   const MsnObject *obj)
{
	MsnSlpCall *slpcall;
	char *msnobj_data;
	char *msnobj_base64;

	g_return_if_fail(slplink != NULL);
	g_return_if_fail(obj     != NULL);

	msnobj_data = msn_object_to_string(obj);
	msnobj_base64 = gaim_base64_encode((const guchar *)msnobj_data, strlen(msnobj_data));
	g_free(msnobj_data);

	slpcall = msn_slp_call_new(slplink);
	msn_slp_call_init(slpcall, MSN_SLPCALL_ANY);

	slpcall->data_info = g_strdup(info);
	slpcall->cb = cb;
	slpcall->end_cb = end_cb;

	msn_slp_call_invite(slpcall, "A4268EEC-FEC5-49E5-95C3-F126696BDBF6", 1,
						msnobj_base64);

	g_free(msnobj_base64);
}
