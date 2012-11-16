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

#ifndef _GGP_LIBGADUW_H
#define _GGP_LIBGADUW_H

#include <internal.h>
#include <libgadu.h>

#include "purplew.h"

typedef void (*ggp_libgaduw_http_cb)(struct gg_http *h, gboolean success,
	gboolean cancelled, gpointer user_data);

typedef struct
{
	gpointer user_data;
	ggp_libgaduw_http_cb cb;
	
	gboolean cancelled;
	struct gg_http *h;
	ggp_purplew_request_processing_handle *processing;
	guint inpa;
} ggp_libgaduw_http_req;

void ggp_libgaduw_setup(void);
void ggp_libgaduw_cleanup(void);

const gchar * ggp_libgaduw_version(PurpleConnection *gc);

ggp_libgaduw_http_req * ggp_libgaduw_http_watch(PurpleConnection *gc,
	struct gg_http *h, ggp_libgaduw_http_cb cb, gpointer user_data,
	gboolean show_processing);
void ggp_libgaduw_http_cancel(ggp_libgaduw_http_req *req);

#endif /* _GGP_LIBGADUW_H */
