/**
 * @file bosh.h Buddy handlers
 *
 * purple
 *
 * Copyright (C) 2008, Tobias Markmann <tmarkmann@googlemail.com>
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
#ifndef _PURPLE_JABBER_BOSH_H_
#define _PURPLE_JABBER_BOSH_H_

typedef struct _PurpleBOSHConnection PurpleBOSHConnection;

#include "jabber.h"

void jabber_bosh_init(void);
void jabber_bosh_uninit(void);

PurpleBOSHConnection* jabber_bosh_connection_init(JabberStream *js, const char *url);
void jabber_bosh_connection_destroy(PurpleBOSHConnection *conn);

gboolean jabber_bosh_connection_is_ssl(PurpleBOSHConnection *conn);

void jabber_bosh_connection_connect(PurpleBOSHConnection *conn);
void jabber_bosh_connection_close(PurpleBOSHConnection *conn);
void jabber_bosh_connection_send(PurpleBOSHConnection *conn, xmlnode *node);
void jabber_bosh_connection_send_raw(PurpleBOSHConnection *conn, const char *data, int len);
void jabber_bosh_connection_refresh(PurpleBOSHConnection *conn);
#endif /* _PURPLE_JABBER_BOSH_H_ */
