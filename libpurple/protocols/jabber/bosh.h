/**
 * @file bosh.h Bidirectional-streams over Synchronous HTTP (BOSH)
 * (XEP-0124 and XEP-0206)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */
#ifndef PURPLE_JABBER_BOSH_H_
#define PURPLE_JABBER_BOSH_H_

typedef struct _PurpleJabberBOSHConnection PurpleJabberBOSHConnection;

#include "jabber.h"

void
jabber_bosh_init(void);

void
jabber_bosh_uninit(void);

PurpleJabberBOSHConnection *
jabber_bosh_connection_new(JabberStream *js, const gchar *url);

void
jabber_bosh_connection_destroy(PurpleJabberBOSHConnection *conn);

gboolean
jabber_bosh_connection_is_ssl(const PurpleJabberBOSHConnection *conn);

void
jabber_bosh_connection_send(PurpleJabberBOSHConnection *conn,
	const gchar *data);

void
jabber_bosh_connection_send_keepalive(PurpleJabberBOSHConnection *conn);

#endif /* PURPLE_JABBER_BOSH_H_ */
