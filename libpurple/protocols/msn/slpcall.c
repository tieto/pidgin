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

/* #define MSN_DEBUG_SLPCALL */

/**************************************************************************
 * Main
 **************************************************************************/

MsnSlpCall *
msn_slp_call_new(MsnSlpLink *slplink)
{
	MsnSlpCall *slpcall;

	g_return_val_if_fail(slplink != NULL, NULL);

	slpcall = g_new0(MsnSlpCall, 1);

#ifdef MSN_DEBUG_SLPCALL
	purple_debug_info("msn", "slpcall_new: slpcall(%p)\n", slpcall);
#endif

	slpcall->slplink = slplink;

	msn_slplink_add_slpcall(slplink, slpcall);

	slpcall->timer = purple_timeout_add(MSN_SLPCALL_TIMEOUT, msn_slp_call_timeout, slpcall);

	return slpcall;
}

void
msn_slp_call_destroy(MsnSlpCall *slpcall)
{
	GList *e;
	MsnSession *session;

#ifdef MSN_DEBUG_SLPCALL
	purple_debug_info("msn", "slpcall_destroy: slpcall(%p)\n", slpcall);
#endif

	g_return_if_fail(slpcall != NULL);

	if (slpcall->timer)
		purple_timeout_remove(slpcall->timer);

	for (e = slpcall->slplink->slp_msgs; e != NULL; )
	{
		MsnSlpMessage *slpmsg = e->data;
		e = e->next;

#ifdef MSN_DEBUG_SLPCALL_VERBOSE
		purple_debug_info("msn", "slpcall_destroy: trying slpmsg(%p)\n",
						slpmsg);
#endif

		if (slpmsg->slpcall == slpcall)
		{
			msn_slpmsg_destroy(slpmsg);
		}
	}

	session = slpcall->slplink->session;

	msn_slplink_remove_slpcall(slpcall->slplink, slpcall);

	if (slpcall->end_cb != NULL)
		slpcall->end_cb(slpcall, session);

	if (slpcall->xfer != NULL) {
		slpcall->xfer->data = NULL;
		purple_xfer_unref(slpcall->xfer);
	}

	g_free(slpcall->id);
	g_free(slpcall->branch);
	g_free(slpcall->data_info);

	g_free(slpcall);
}

void
msn_slp_call_init(MsnSlpCall *slpcall, MsnSlpCallType type)
{
	slpcall->session_id = rand() % 0xFFFFFF00 + 4;
	slpcall->id = rand_guid();
	slpcall->type = type;
}

void
msn_slp_call_session_init(MsnSlpCall *slpcall)
{
	if (slpcall->session_init_cb)
		slpcall->session_init_cb(slpcall);

	slpcall->started = TRUE;
}

void
msn_slp_call_invite(MsnSlpCall *slpcall, const char *euf_guid,
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

#ifdef MSN_DEBUG_SLP
	slpmsg->info = "SLP INVITE";
	slpmsg->text_body = TRUE;
#endif

	msn_slplink_send_slpmsg(slplink, slpmsg);

	g_free(header);
	g_free(content);
}

void
msn_slp_call_close(MsnSlpCall *slpcall)
{
	g_return_if_fail(slpcall != NULL);
	g_return_if_fail(slpcall->slplink != NULL);

	send_bye(slpcall, "application/x-msnmsgr-sessionclosebody");
	msn_slplink_unleash(slpcall->slplink);
	msn_slp_call_destroy(slpcall);
}

gboolean
msn_slp_call_timeout(gpointer data)
{
	MsnSlpCall *slpcall;

	slpcall = data;

#ifdef MSN_DEBUG_SLPCALL
	purple_debug_info("msn", "slpcall_timeout: slpcall(%p)\n", slpcall);
#endif

	if (!slpcall->pending && !slpcall->progress)
	{
		msn_slp_call_destroy(slpcall);
		return FALSE;
	}

	slpcall->progress = FALSE;

	return TRUE;
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

	if (slpmsg->flags == 0x0)
	{
		char *body_str;

		body_str = g_strndup((const char *)body, body_len);
		slpcall = msn_slp_sip_recv(slplink, body_str);
		g_free(body_str);
	}
	else if (slpmsg->flags == 0x20 || slpmsg->flags == 0x1000030)
	{
		slpcall = msn_slplink_find_slp_call_with_session_id(slplink, slpmsg->session_id);

		if (slpcall != NULL)
		{
			if (slpcall->timer)
				purple_timeout_remove(slpcall->timer);

			slpcall->cb(slpcall, body, body_len);

			slpcall->wasted = TRUE;
		}
	}
#if 0
	else if (slpmsg->flags == 0x100)
	{
		slpcall = slplink->directconn->initial_call;

		if (slpcall != NULL)
			msn_slp_call_session_init(slpcall);
	}
#endif

	return slpcall;
}
