/**
 * @file gg.h
 *
 * purple
 *
 * Copyright (C) 2005  Bartosz Oler <bartosz@bzimage.us>
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


#ifndef _PURPLE_GG_H
#define _PURPLE_GG_H

#include <libgadu.h>
#include "internal.h"
#include "search.h"
#include "connection.h"

#include "image.h"
#include "avatar.h"
#include "account.h"
#include "roster.h"
#include "multilogon.h"
#include "status.h"

#define PUBDIR_RESULTS_MAX 20

#define GGP_UIN_LEN_MAX 10

#define GGP_ID     "prpl-gg"
#define GGP_NAME   "Gadu-Gadu"
#define GGP_DOMAIN (g_quark_from_static_string(GGP_ID))

#define GGP_TYPE_PROTOCOL             (ggp_protocol_get_type())
#define GGP_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GGP_TYPE_PROTOCOL, GaduGaduProtocol))
#define GGP_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GGP_TYPE_PROTOCOL, GaduGaduProtocolClass))
#define GGP_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GGP_TYPE_PROTOCOL))
#define GGP_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GGP_TYPE_PROTOCOL))
#define GGP_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GGP_TYPE_PROTOCOL, GaduGaduProtocolClass))

typedef struct _GaduGaduProtocol
{
	PurpleProtocol parent;
} GaduGaduProtocol;

typedef struct _GaduGaduProtocolClass
{
	PurpleProtocolClass parent_class;
} GaduGaduProtocolClass;

typedef struct
{
	char *name;
	GList *participants;

} GGPChat;

typedef struct {

	struct gg_session *session;
	guint inpa;
	GList *chats;
	int chats_count;

	ggp_image_connection_data image_data;
	ggp_avatar_session_data avatar_data;
	ggp_roster_session_data roster_data;
	ggp_multilogon_session_data *multilogon_data;
	ggp_status_session_data *status_data;
} GGPInfo;

typedef struct
{
	gboolean blocked;
} ggp_buddy_data;

/**
 * Returns the GType for the GaduGaduProtocol object.
 */
GType ggp_protocol_get_type(void);

void ggp_recv_message_handler(PurpleConnection *gc, const struct gg_event_msg *ev, gboolean multilogon);

ggp_buddy_data * ggp_buddy_get_data(PurpleBuddy *buddy);

#endif /* _PURPLE_GG_H */
