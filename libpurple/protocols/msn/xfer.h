/**
 * @file xfer.h MSN File Transfer functions
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


#include "slpcall.h"

void msn_xfer_init(PurpleXfer *xfer);
void msn_xfer_cancel(PurpleXfer *xfer);

gssize msn_xfer_write(const guchar *data, gsize len, PurpleXfer *xfer);
gssize msn_xfer_read(guchar **data, PurpleXfer *xfer);

void msn_xfer_completed_cb(MsnSlpCall *slpcall,
						   const guchar *body, gsize size);
void msn_xfer_end_cb(MsnSlpCall *slpcall, MsnSession *session);
