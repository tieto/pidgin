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

#ifdef USE_FARSIGHT

#include <farsight/farsight.h>

G_BEGIN_DECLS

typedef struct {
	char *id;
	JabberStream *js;
	FarsightStream *stream;
	PurpleMedia *media;
	char *remote_jid;
	char *initiator;
	gboolean is_initiator;
	GList *remote_candidates;
    GList *remote_codecs;
} JingleSession;

JingleSession *jabber_jingle_session_create(JabberStream *js);
JingleSession *jabber_jingle_session_create_by_id(JabberStream *js, 
												  const char *id);

const char *jabber_jingle_session_get_id(const JingleSession *sess);
JabberStream *jabber_jingle_session_get_js(const JingleSession *sess);

void jabber_jingle_session_destroy(JingleSession *sess);

JingleSession *jabber_jingle_session_find_by_id(const char *id);

FarsightStream *jabber_jingle_session_get_stream(const JingleSession *sess);
void jabber_jingle_session_set_stream(JingleSession *sess, FarsightStream *stream);

PurpleMedia *jabber_jingle_session_get_media(const JingleSession *sess);
void jabber_jingle_session_set_media(JingleSession *sess, PurpleMedia *media);

const char *jabber_jingle_session_get_remote_jid(const JingleSession *sess);

gboolean jabber_jingle_session_is_initiator(const JingleSession *sess);

void jabber_jingle_session_set_remote_jid(JingleSession *sess, 
										  const char *remote_jid);

const char *jabber_jingle_session_get_initiator(const JingleSession *sess);
void jabber_jingle_session_set_initiator(JingleSession *sess, 
										 const char *initiator);

void jabber_jingle_session_add_remote_candidate(JingleSession *sess,
												const GList *candidate);

xmlnode *jabber_jingle_session_create_terminate(const JingleSession *sess,
												const char *reasoncode,
												const char *reasontext);

xmlnode *jabber_jingle_session_create_session_accept(const JingleSession *sess);
xmlnode *jabber_jingle_session_create_transport_info(const JingleSession *sess,
													 gchar *candidate_id);
xmlnode *jabber_jingle_session_create_content_replace(const JingleSession *sess,
													  gchar *native_candidate_id,
													  gchar *remote_candidate_id);
xmlnode *jabber_jingle_session_create_content_accept(const JingleSession *sess);

/**
 * Gets a list of Farsight codecs from a Jingle <description> tag
 *
 * @param description
 * @return A GList of FarsightCodecS
*/
GList *jabber_jingle_get_codecs(const xmlnode *description);

GList *jabber_jingle_get_candidates(const xmlnode *transport);

void jabber_jingle_session_set_remote_codecs(JingleSession *sess,
                                             GList *codecs);
GList *jabber_jingle_session_get_remote_codecs(const JingleSession *sess);

G_END_DECLS

#endif /* USE_FARSIGHT */

#endif /* JINGLE_H */
