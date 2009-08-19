/**
 * @file slpcall.c SLP Call Functions
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
#include "msnutils.h"
#include "slpcall.h"

#include "slp.h"

/**************************************************************************
 * Main
 **************************************************************************/

static gboolean
msn_slpcall_timeout(gpointer data)
{
	MsnSlpCall *slpcall;

	slpcall = data;

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "slpcall_timeout: slpcall(%p)\n", slpcall);

	if (!slpcall->pending && !slpcall->progress)
	{
		msn_slpcall_destroy(slpcall);
		return TRUE;
	}

	slpcall->progress = FALSE;

	return TRUE;
}

MsnSlpCall *
msn_slpcall_new(MsnSlpLink *slplink)
{
	MsnSlpCall *slpcall;

	g_return_val_if_fail(slplink != NULL, NULL);

	slpcall = g_new0(MsnSlpCall, 1);

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "slpcall_new: slpcall(%p)\n", slpcall);

	slpcall->slplink = slplink;

	msn_slplink_add_slpcall(slplink, slpcall);

	slpcall->timer = purple_timeout_add_seconds(MSN_SLPCALL_TIMEOUT, msn_slpcall_timeout, slpcall);

	return slpcall;
}

void
msn_slpcall_destroy(MsnSlpCall *slpcall)
{
	GList *e;

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "slpcall_destroy: slpcall(%p)\n", slpcall);

	g_return_if_fail(slpcall != NULL);

	if (slpcall->timer)
		purple_timeout_remove(slpcall->timer);

	for (e = slpcall->slplink->slp_msgs; e != NULL; )
	{
		MsnSlpMessage *slpmsg = e->data;
		e = e->next;

		if (purple_debug_is_verbose())
			purple_debug_info("msn", "slpcall_destroy: trying slpmsg(%p)\n",
			                  slpmsg);

		if (slpmsg->slpcall == slpcall)
		{
			msn_slpmsg_destroy(slpmsg);
		}
	}

	if (slpcall->end_cb != NULL)
		slpcall->end_cb(slpcall, slpcall->slplink->session);

	if (slpcall->xfer != NULL) {
		slpcall->xfer->data = NULL;
		purple_xfer_unref(slpcall->xfer);
	}

	msn_slplink_remove_slpcall(slpcall->slplink, slpcall);

	g_free(slpcall->id);
	g_free(slpcall->branch);
	g_free(slpcall->data_info);

	g_free(slpcall);
}

void
msn_slpcall_init(MsnSlpCall *slpcall, MsnSlpCallType type)
{
	slpcall->session_id = rand() % 0xFFFFFF00 + 4;
	slpcall->id = rand_guid();
	slpcall->type = type;
}

void
msn_slpcall_session_init(MsnSlpCall *slpcall)
{
	if (slpcall->session_init_cb)
		slpcall->session_init_cb(slpcall);

	slpcall->started = TRUE;
}

void
msn_slpcall_invite(MsnSlpCall *slpcall, const char *euf_guid,
					int app_id, const char *context)
{
	MsnSlpLink *slplink;
	MsnSlpMessage *slpmsg;
	char *header;
	char *content;

	g_return_if_fail(slpcall != NULL);
	g_return_if_fail(context != NULL);

	slplink = slpcall->slplink;

	slpcall->branch = rand_guid();

	content = g_strdup_printf(
		"EUF-GUID: {%s}\r\n"
		"SessionID: %lu\r\n"
		"AppID: %d\r\n"
		"Context: %s\r\n\r\n",
		euf_guid,
		slpcall->session_id,
		app_id,
		context);

	header = g_strdup_printf("INVITE MSNMSGR:%s MSNSLP/1.0",
							 slplink->remote_user);

	slpmsg = msn_slpmsg_sip_new(slpcall, 0, header, slpcall->branch,
								"application/x-msnmsgr-sessionreqbody", content);

	slpmsg->info = "SLP INVITE";
	slpmsg->text_body = TRUE;

	msn_slplink_send_slpmsg(slplink, slpmsg);

	g_free(header);
	g_free(content);
}

void
msn_slpcall_close(MsnSlpCall *slpcall)
{
	g_return_if_fail(slpcall != NULL);
	g_return_if_fail(slpcall->slplink != NULL);

	send_bye(slpcall, "application/x-msnmsgr-sessionclosebody");
	msn_slplink_send_queued_slpmsgs(slpcall->slplink);
	msn_slpcall_destroy(slpcall);
}

MsnSlpCall *
msn_slp_process_msg(MsnSlpLink *slplink, MsnSlpMessage *slpmsg)
{
	MsnSlpCall *slpcall;
	const guchar *body;
	gsize body_len;

	slpcall = NULL;
	body = slpmsg->buffer;
	body_len = slpmsg->size;

	if (slpmsg->flags == 0x0 || slpmsg->flags == 0x1000000)
	{
		char *body_str;

		if (slpmsg->session_id == 64)
		{
			/* This is for handwritten messages (Ink) */
			GError *error;
			gsize bytes_read, bytes_written;

			body_str = g_convert((const gchar *)body, body_len / 2,
			                     "UTF-8", "UTF-16LE",
			                     &bytes_read, &bytes_written, &error);
			body_len -= bytes_read + 2;
			body += bytes_read + 2;
			if (body_str == NULL
			 || body_len <= 0
			 || strstr(body_str, "image/gif") == NULL)
			{
				if (error != NULL) {
					purple_debug_error("msn",
					                   "Unable to convert Ink header from UTF-16 to UTF-8: %s\n",
					                   error->message);
					g_error_free(error);
				}
				else
					purple_debug_error("msn",
					                   "Received Ink in unknown format\n");
				g_free(body_str);
				return NULL;
			}
			g_free(body_str);

			body_str = g_convert((const gchar *)body, body_len / 2,
			                     "UTF-8", "UTF16-LE",
			                     &bytes_read, &bytes_written, &error);
			if (!body_str)
			{
				if (error != NULL) {
					purple_debug_error("msn",
					                   "Unable to convert Ink body from UTF-16 to UTF-8: %s\n",
					                   error->message);
					g_error_free(error);
				}
				else
					purple_debug_error("msn",
					                   "Received Ink in unknown format\n");
				return NULL;
			}

			msn_switchboard_show_ink(slpmsg->slplink->swboard,
			                         slplink->remote_user,
			                         body_str);
		}
		else
		{
			body_str = g_strndup((const char *)body, body_len);
			slpcall = msn_slp_sip_recv(slplink, body_str);
		}
		g_free(body_str);
	}
	else if (slpmsg->flags == 0x20 ||
	         slpmsg->flags == 0x1000020 ||
	         slpmsg->flags == 0x1000030)
	{
		slpcall = msn_slplink_find_slp_call_with_session_id(slplink, slpmsg->session_id);

		if (slpcall != NULL)
		{
			if (slpcall->timer) {
				purple_timeout_remove(slpcall->timer);
				slpcall->timer = 0;
			}

			slpcall->cb(slpcall, body, body_len);

			slpcall->wasted = TRUE;
		}
	}
#if 0
	else if (slpmsg->flags == 0x100)
	{
		slpcall = slplink->directconn->initial_call;

		if (slpcall != NULL)
			msn_slpcall_session_init(slpcall);
	}
#endif
	else if (slpmsg->flags == 0x2)
	{
		/* Acknowledgement of previous message. Don't do anything currently. */
	}
	else
		purple_debug_warning("msn", "Unprocessed SLP message with flags 0x%08lx\n",
		                     slpmsg->flags);

	return slpcall;
}
