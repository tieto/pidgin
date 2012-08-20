/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#include "libgaduw.h"

#include <debug.h>

#include "purplew.h"

/*******************************************************************************
 * HTTP requests.
 ******************************************************************************/

static void ggp_libgaduw_http_processing_cancel(PurpleConnection *gc,
	void *_req);

static void ggp_libgaduw_http_handler(gpointer _req, gint fd,
	PurpleInputCondition cond);

static void ggp_libgaduw_http_finish(ggp_libgaduw_http_req *req,
	gboolean success);

/******************************************************************************/

ggp_libgaduw_http_req * ggp_libgaduw_http_watch(PurpleConnection *gc,
	struct gg_http *h, ggp_libgaduw_http_cb cb,
	gpointer user_data, gboolean show_processing)
{
	ggp_libgaduw_http_req *req;
	purple_debug_misc("gg", "ggp_libgaduw_http_watch(h=%x, "
		"show_processing=%d)\n", (unsigned int)h, show_processing);
	
	req = g_new(ggp_libgaduw_http_req, 1);
	req->user_data = user_data;
	req->cb = cb;
	req->cancelled = FALSE;
	req->h = h;
	req->processing = NULL;
	if (show_processing)
		req->processing = ggp_purplew_request_processing(gc, NULL,
			req, ggp_libgaduw_http_processing_cancel);
	req->inpa = ggp_purplew_http_input_add(h, ggp_libgaduw_http_handler,
		req);
	
	return req;
}

static void ggp_libgaduw_http_processing_cancel(PurpleConnection *gc,
	void *_req)
{
	ggp_libgaduw_http_req *req = _req;
	req->processing = NULL;
	ggp_libgaduw_http_cancel(req);
}

static void ggp_libgaduw_http_handler(gpointer _req, gint fd,
	PurpleInputCondition cond)
{
	ggp_libgaduw_http_req *req = _req;
	
	if (req->h->callback(req->h) == -1 || req->h->state == GG_STATE_ERROR)
	{
		purple_debug_error("gg", "ggp_libgaduw_http_handler: failed to "
			"make http request: %d\n", req->h->error);
		ggp_libgaduw_http_finish(req, FALSE);
		return;
	}
	
	//TODO: verbose mode
	//purple_debug_misc("gg", "ggp_libgaduw_http_handler: got fd update "
	//	"[check=%d, state=%d]\n", req->h->check, req->h->state);
	
	if (req->h->state != GG_STATE_DONE)
	{
		purple_input_remove(req->inpa);
		req->inpa = ggp_purplew_http_input_add(req->h,
			ggp_libgaduw_http_handler, req);
		return;
	}
	
	if (!req->h->data || !req->h->body)
	{
		purple_debug_error("gg", "ggp_libgaduw_http_handler: got empty "
			"http response: %d\n", req->h->error);
		ggp_libgaduw_http_finish(req, FALSE);
		return;
	}

	ggp_libgaduw_http_finish(req, TRUE);
}

void ggp_libgaduw_http_cancel(ggp_libgaduw_http_req *req)
{
	purple_debug_misc("gg", "ggp_libgaduw_http_cancel\n");
	req->cancelled = TRUE;
	gg_http_stop(req->h);
	ggp_libgaduw_http_finish(req, FALSE);
}

static void ggp_libgaduw_http_finish(ggp_libgaduw_http_req *req,
	gboolean success)
{
	purple_debug_misc("gg", "ggp_libgaduw_http_finish(h=%x, processing=%x):"
		" success=%d\n", (unsigned int)req->h,
		(unsigned int)req->processing, success);
	if (req->processing)
	{
		ggp_purplew_request_processing_done(req->processing);
		req->processing = NULL;
	}
	purple_input_remove(req->inpa);
	req->cb(req->h, success, req->cancelled, req->user_data);
	req->h->destroy(req->h);
	g_free(req);
}
