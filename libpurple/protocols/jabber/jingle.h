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

#ifdef USE_VV

G_BEGIN_DECLS

typedef struct {
	char *id;
	JabberStream *js;
	PurpleMedia *media;
	char *remote_jid;
	char *initiator;
	gboolean is_initiator;
	gboolean session_started;
} JingleSession;

JingleSession *jabber_jingle_session_create(JabberStream *js);
JingleSession *jabber_jingle_session_create_by_id(JabberStream *js, 
												  const char *id);

const char *jabber_jingle_session_get_id(const JingleSession *sess);
JabberStream *jabber_jingle_session_get_js(const JingleSession *sess);

void jabber_jingle_session_destroy(JingleSession *sess);

JingleSession *jabber_jingle_session_find_by_id(JabberStream *js, const char *id);
JingleSession *jabber_jingle_session_find_by_jid(JabberStream *js, const char *jid);

PurpleMedia *jabber_jingle_session_get_media(const JingleSession *sess);
void jabber_jingle_session_set_media(JingleSession *sess, PurpleMedia *media);

const char *jabber_jingle_session_get_remote_jid(const JingleSession *sess);

gboolean jabber_jingle_session_is_initiator(const JingleSession *sess);

void jabber_jingle_session_set_remote_jid(JingleSession *sess, 
										  const char *remote_jid);

const char *jabber_jingle_session_get_initiator(const JingleSession *sess);
void jabber_jingle_session_set_initiator(JingleSession *sess, 
										 const char *initiator);

xmlnode *jabber_jingle_session_create_terminate(const JingleSession *sess,
												const char *reasoncode,
												const char *reasontext);
xmlnode *jabber_jingle_session_create_session_accept(const JingleSession *sess);
xmlnode *jabber_jingle_session_create_transport_info(const JingleSession *sess);
xmlnode *jabber_jingle_session_create_content_replace(const JingleSession *sess,
						      FsCandidate *native_candidate,
						      FsCandidate *remote_candidate);
xmlnode *jabber_jingle_session_create_content_accept(const JingleSession *sess);
xmlnode *jabber_jingle_session_create_description(const JingleSession *sess);

/**
 * Gets a list of Farsight codecs from a Jingle <description> tag
 *
 * @param description
 * @return A GList of FarsightCodecS
*/
GList *jabber_jingle_get_codecs(const xmlnode *description);

GList *jabber_jingle_get_candidates(const xmlnode *transport);

PurpleMedia *jabber_jingle_session_initiate_media(PurpleConnection *gc,
						  const char *who,
						  PurpleMediaStreamType type);

/* Jingle message handlers */
void jabber_jingle_session_handle_content_replace(JabberStream *js, xmlnode *packet);
void jabber_jingle_session_handle_session_accept(JabberStream *js, xmlnode *packet);
void jabber_jingle_session_handle_session_initiate(JabberStream *js, xmlnode *packet);
void jabber_jingle_session_handle_session_terminate(JabberStream *js, xmlnode *packet);
void jabber_jingle_session_handle_transport_info(JabberStream *js, xmlnode *packet);

G_END_DECLS

#endif /* USE_VV */

#endif /* JINGLE_H */
