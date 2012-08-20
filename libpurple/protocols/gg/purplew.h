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

#ifndef _GGP_PURPLEW_H
#define _GGP_PURPLEW_H

#include <internal.h>
#include <libgadu.h>

#define GGP_PURPLEW_GROUP_DEFAULT _("Buddies")

/**
 * Adds an input handler in purple event loop for http request.
 *
 * @see purple_input_add
 *
 * @param http_req  Http connection to watch.
 * @param func      The callback function for data.
 * @param user_data User-specified data.
 *
 * @return The resulting handle (will be greater than 0).
 */
guint ggp_purplew_http_input_add(struct gg_http *http_req,
	PurpleInputFunction func, gpointer user_data);

typedef void (*ggp_purplew_request_processing_cancel_cb)(PurpleConnection *gc,
	void *user_data);

typedef struct
{
	PurpleConnection *gc;
	ggp_purplew_request_processing_cancel_cb cancel_cb;
	void *request_handle;
	void *user_data;
} ggp_purplew_request_processing_handle;

ggp_purplew_request_processing_handle * ggp_purplew_request_processing(
	PurpleConnection *gc, const gchar *msg, void *user_data,
	ggp_purplew_request_processing_cancel_cb oncancel);

void ggp_purplew_request_processing_done(
	ggp_purplew_request_processing_handle *handle);

// ignores default group
PurpleGroup * ggp_purplew_buddy_get_group_only(PurpleBuddy *buddy);

GList * ggp_purplew_group_get_buddies(PurpleGroup *group, PurpleAccount *account);

// you must g_free returned list
GList * ggp_purplew_account_get_groups(PurpleAccount *account, gboolean exclusive);

#endif /* _GGP_PURPLEW_H */
