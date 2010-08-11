/**
 * @file auth.h Authentication routines
 *
 * purple
 *
 * Copyright (C) 2003 Nathan Walp <faceprint@faceprint.com>
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
#ifndef PURPLE_JABBER_AUTH_H_
#define PURPLE_JABBER_AUTH_H_

#include "jabber.h"
#include "xmlnode.h"

gboolean jabber_process_starttls(JabberStream *js, xmlnode *packet);
void jabber_auth_start(JabberStream *js, xmlnode *packet);
void jabber_auth_start_old(JabberStream *js);
void jabber_auth_handle_challenge(JabberStream *js, xmlnode *packet);
void jabber_auth_handle_success(JabberStream *js, xmlnode *packet);
void jabber_auth_handle_failure(JabberStream *js, xmlnode *packet);

#endif /* PURPLE_JABBER_AUTH_H_ */
