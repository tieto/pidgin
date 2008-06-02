/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
 
#ifndef JINGLE_H
#define JINGLE_H

#include "config.h"
#include "jabber.h"
#include "media.h"

#include <glib.h>
#include <glib-object.h>

/*
 * When Jingle content types other than voice and video are implemented,
 * this #ifdef others surrounding Jingle code should be changed to just
 * be around the voice and video specific parts.
 */
#ifdef USE_VV

G_BEGIN_DECLS

PurpleMedia *jabber_jingle_session_initiate_media(PurpleConnection *gc,
						  const char *who,
						  PurpleMediaStreamType type);

void jabber_jingle_session_terminate_session_media(JabberStream *js, const gchar *who);
void jabber_jingle_session_terminate_sessions(JabberStream *js);

/* Jingle message handlers */
void jabber_jingle_session_handle_content_replace(JabberStream *js, xmlnode *packet);
void jabber_jingle_session_handle_session_accept(JabberStream *js, xmlnode *packet);
void jabber_jingle_session_handle_session_initiate(JabberStream *js, xmlnode *packet);
void jabber_jingle_session_handle_session_terminate(JabberStream *js, xmlnode *packet);
void jabber_jingle_session_handle_transport_info(JabberStream *js, xmlnode *packet);

G_END_DECLS

#endif /* USE_VV */

#endif /* JINGLE_H */
