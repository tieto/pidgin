/* purple
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
/**
 * SECTION:candidate
 * @section_id: libpurple-candidate
 * @title: media/candidate.h
 * @short_description: Candidate for Media API
 */

#ifndef _PURPLE_MEDIA_CANDIDATE_H_
#define _PURPLE_MEDIA_CANDIDATE_H_

#include "enum-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define PURPLE_TYPE_MEDIA_CANDIDATE            (purple_media_candidate_get_type())
#define PURPLE_IS_MEDIA_CANDIDATE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MEDIA_CANDIDATE))
#define PURPLE_IS_MEDIA_CANDIDATE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_MEDIA_CANDIDATE))
#define PURPLE_MEDIA_CANDIDATE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MEDIA_CANDIDATE, PurpleMediaCandidate))
#define PURPLE_MEDIA_CANDIDATE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MEDIA_CANDIDATE, PurpleMediaCandidate))
#define PURPLE_MEDIA_CANDIDATE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MEDIA_CANDIDATE, PurpleMediaCandidate))

/**
 * PurpleMediaCandidate:
 *
 * An opaque structure representing a network candidate (IP Address and port
 * pair).
 */
typedef struct _PurpleMediaCandidate PurpleMediaCandidate;

/**
 * purple_media_candidate_get_type:
 *
 * Gets the type of the media candidate structure.
 *
 * Returns: The media canditate's GType
 */
GType purple_media_candidate_get_type(void);

/**
 * purple_media_candidate_new:
 * @foundation: The foundation of the candidate.
 * @component_id: The component this candidate is for.
 * @type: The type of candidate.
 * @proto: The protocol this component is for.
 * @ip: The IP address of this component.
 * @port: The network port.
 *
 * Creates a PurpleMediaCandidate instance.
 *
 * Returns: The newly created PurpleMediaCandidate instance.
 */
PurpleMediaCandidate *purple_media_candidate_new(
		const gchar *foundation, guint component_id,
		PurpleMediaCandidateType type,
		PurpleMediaNetworkProtocol proto,
		const gchar *ip, guint port);

/**
 * purple_media_candidate_copy:
 * @candidate: The candidate to copy.
 *
 * Copies a PurpleMediaCandidate.
 *
 * Returns: The copy of the PurpleMediaCandidate.
 */
PurpleMediaCandidate *purple_media_candidate_copy(
		PurpleMediaCandidate *candidate);

/**
 * purple_media_candidate_list_copy:
 * @candidates: The list of candidates to be copied.
 *
 * Copies a GList of PurpleMediaCandidate and its contents.
 *
 * Returns: The copy of the GList.
 */
GList *purple_media_candidate_list_copy(GList *candidates);

/**
 * purple_media_candidate_list_free:
 * @candidates: The list of candidates to be freed.
 *
 * Frees a GList of PurpleMediaCandidate and its contents.
 */
void purple_media_candidate_list_free(GList *candidates);

/**
 * purple_media_candidate_get_foundation:
 * @candidate: The candidate to get the foundation from.
 *
 * Gets the foundation (identifier) from the candidate.
 *
 * Returns: The foundation.
 */
gchar *purple_media_candidate_get_foundation(PurpleMediaCandidate *candidate);

/**
 * purple_media_candidate_get_component_id:
 * @candidate: The candidate to get the compnent id from.
 *
 * Gets the component id (rtp or rtcp)
 *
 * Returns: The component id.
 */
guint purple_media_candidate_get_component_id(PurpleMediaCandidate *candidate);

/**
 * purple_media_candidate_get_ip:
 * @candidate: The candidate to get the IP address from.
 *
 * Gets the IP address.
 *
 * Returns: The IP address.
 */
gchar *purple_media_candidate_get_ip(PurpleMediaCandidate *candidate);

/**
 * purple_media_candidate_get_port:
 * @candidate: The candidate to get the port from.
 *
 * Gets the port.
 *
 * Returns: The port.
 */
guint16 purple_media_candidate_get_port(PurpleMediaCandidate *candidate);

/**
 * purple_media_candidate_get_base_ip:
 * @candidate: The candidate to get the base IP address from.
 *
 * Gets the base (internal) IP address.
 * This can be NULL.
 *
 * Returns: The base IP address.
 */
gchar *purple_media_candidate_get_base_ip(PurpleMediaCandidate *candidate);

/**
 * purple_media_candidate_get_base_port:
 * @candidate: The candidate to get the base port.
 *
 * Gets the base (internal) port.
 * Invalid if the base IP is NULL.
 *
 * Returns: The base port.
 */
guint16 purple_media_candidate_get_base_port(PurpleMediaCandidate *candidate);

/**
 * purple_media_candidate_get_protocol:
 * @candidate: The candidate to get the protocol from.
 *
 * Gets the protocol (TCP or UDP).
 *
 * Returns: The protocol.
 */
PurpleMediaNetworkProtocol purple_media_candidate_get_protocol(
		PurpleMediaCandidate *candidate);

/**
 * purple_media_candidate_get_priority:
 * @candidate: The candidate to get the priority from.
 *
 * Gets the priority.
 *
 * Returns: The priority.
 */
guint32 purple_media_candidate_get_priority(PurpleMediaCandidate *candidate);

/**
 * purple_media_candidate_get_candidate_type:
 * @candidate: The candidate to get the candidate type from.
 *
 * Gets the candidate type.
 *
 * Returns: The candidate type.
 */
PurpleMediaCandidateType purple_media_candidate_get_candidate_type(
		PurpleMediaCandidate *candidate);

/**
 * purple_media_candidate_get_username:
 * @candidate: The candidate to get the username from.
 *
 * Gets the username.
 * This can be NULL. It depends on the transmission type.
 *
 * Returns: The username.
 */
gchar *purple_media_candidate_get_username(PurpleMediaCandidate *candidate);

/**
 * purple_media_candidate_get_password:
 * @candidate: The candidate to get the password from.
 *
 * Gets the password.
 *
 * This can be NULL. It depends on the transmission type.
 *
 * Returns: The password.
 */
gchar *purple_media_candidate_get_password(PurpleMediaCandidate *candidate);

/**
 * purple_media_candidate_get_ttl:
 * @candidate: The candidate to get the TTL from.
 *
 * Gets the TTL.
 *
 * Returns: The TTL.
 */
guint purple_media_candidate_get_ttl(PurpleMediaCandidate *candidate);

G_END_DECLS

#endif  /* _PURPLE_MEDIA_CANDIDATE_H_ */

