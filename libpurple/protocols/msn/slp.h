/**
 * @file slp.h MSNSLP support
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
#ifndef MSN_SLP_H
#define MSN_SLP_H

#include "internal.h"
#include "ft.h"

#include "session.h"
#include "slpcall.h"
#include "slplink.h"
#include "user.h"

#define MAX_FILE_NAME_LEN 260 /* MAX_PATH in Windows */

/**
 * The context data for a file transfer request
 */
#pragma pack(push,1) /* Couldn't they have made it the right size? */
typedef struct
{
	guint32   length;       /*< Length of header */
	guint32   version;      /*< MSN version */
	guint64   file_size;    /*< Size of file */
	guint32   type;         /*< Transfer type */
	gunichar2 file_name[MAX_FILE_NAME_LEN]; /*< Self-explanatory */
	gchar     unknown1[30]; /*< Used somehow for background sharing */
	guint32   unknown2;     /*< Possibly for background sharing as well */
	gchar     preview[1];   /*< File preview data, 96x96 PNG */
} MsnFileContext;
#pragma pack(pop)

void
msn_slp_send_ok(MsnSlpCall *slpcall, const char *branch,
		const char *type, const char *content);

void
msn_slp_send_decline(MsnSlpCall *slpcall, const char *branch,
			 const char *type, const char *content);


void send_bye(MsnSlpCall *slpcall, const char *type);


void msn_request_user_display(MsnUser *user);

void msn_request_ft(PurpleXfer *xfer);

#endif /* MSN_SLP_H */
