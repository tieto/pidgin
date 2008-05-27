/*
 * purple - Jabber Protocol Plugin
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
 *
 */

#include "config.h"
#include "purple.h"
#include "jingle.h"
#include "xmlnode.h"

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#ifdef USE_FARSIGHT

#include <farsight/farsight.h>
#include <farsight/farsight-transport.h>

/* keep a hash table of JingleSessions */
static GHashTable *sessions = NULL;

static gboolean
jabber_jingle_session_equal(gconstpointer a, gconstpointer b)
{
	purple_debug_info("jingle", 
					  "jabber_jingle_session_equal, comparing %s and %s\n",
					  ((JingleSession *)a)->id,
					  ((JingleSession *)b)->id);
	return !strcmp(((JingleSession *) a)->id, ((JingleSession *) b)->id);
}

static JingleSession *
jabber_jingle_session_create_internal(JabberStream *js,
									  const char *id)
{
    JingleSession *sess = g_new0(JingleSession, 1);
	sess->js = js;
		
	if (id) {
		sess->id = g_strdup(id);
	} else if (js) {
		/* init the session ID... */
		sess->id = jabber_get_next_id(js);
	}
	
	/* insert it into the hash table */
	if (!sessions) {
		purple_debug_info("jingle", "Creating hash table for sessions\n");
		sessions = g_hash_table_new(g_str_hash, g_str_equal);
	}
	purple_debug_info("jingle", "inserting session with key: %s into table\n",
					  sess->id);
	g_hash_table_insert(sessions, sess->id, sess);
	
	sess->remote_candidates = NULL;
    sess->remote_codecs = NULL;
	
	return sess;
}

JabberStream *
jabber_jingle_session_get_js(const JingleSession *sess)
{
	return sess->js;
}

JingleSession *
jabber_jingle_session_create(JabberStream *js)
{
	JingleSession *sess = jabber_jingle_session_create_internal(js, NULL);
	sess->is_initiator = TRUE;	
	return sess;
}

JingleSession *
jabber_jingle_session_create_by_id(JabberStream *js, const char *id)
{
	JingleSession *sess = jabber_jingle_session_create_internal(js, id);
	sess->is_initiator = FALSE;
	return sess;
}

const char *
jabber_jingle_session_get_id(const JingleSession *sess)
{
	return sess->id;
}

void
jabber_jingle_session_destroy(JingleSession *sess)
{
	g_hash_table_remove(sessions, sess->id);
	g_free(sess->id);
    farsight_codec_list_destroy(sess->remote_codecs);
    g_list_free(sess->remote_candidates);
	g_free(sess);
}

JingleSession *
jabber_jingle_session_find_by_id(const char *id)
{
	purple_debug_info("jingle", "find_by_id %s\n", id);
	purple_debug_info("jingle", "hash table: %lx\n", sessions);
	purple_debug_info("jingle", "hash table size %d\n",
					  g_hash_table_size(sessions));
	purple_debug_info("jingle", "lookup: %lx\n", g_hash_table_lookup(sessions, id));  
	return (JingleSession *) g_hash_table_lookup(sessions, id);
}

GList *
jabber_jingle_get_codecs(const xmlnode *description)
{
	GList *codecs = NULL;
	xmlnode *codec_element = NULL;
	const char *encoding_name,*id, *clock_rate;
	FarsightCodec *codec;
	
	for (codec_element = xmlnode_get_child(description, "payload-type") ;
		 codec_element ;
		 codec_element = xmlnode_get_next_twin(codec_element)) {
		encoding_name = xmlnode_get_attrib(codec_element, "name");
		id = xmlnode_get_attrib(codec_element, "id");
		clock_rate = xmlnode_get_attrib(codec_element, "clockrate");

		codec = g_new0(FarsightCodec, 1);
		farsight_codec_init(codec, atoi(id), encoding_name, 
							FARSIGHT_MEDIA_TYPE_AUDIO, 
							clock_rate ? atoi(clock_rate) : 0);
		codecs = g_list_append(codecs, codec);		 
	}
	return codecs;
}

GList *
jabber_jingle_get_candidates(const xmlnode *transport)
{
	GList *candidates = NULL;
	xmlnode *candidate = NULL;
	FarsightTransportInfo *ti;
	
	for (candidate = xmlnode_get_child(transport, "candidate") ;
		 candidate ;
		 candidate = xmlnode_get_next_twin(candidate)) {
		const char *type = xmlnode_get_attrib(candidate, "type");
		ti = g_new0(FarsightTransportInfo, 1);
		ti->component = atoi(xmlnode_get_attrib(candidate, "component"));
		ti->ip = xmlnode_get_attrib(candidate, "ip");
		ti->port = atoi(xmlnode_get_attrib(candidate, "port"));
		ti->proto = strcmp(xmlnode_get_attrib(candidate, "protocol"),
							  "udp") == 0 ? 
				 				FARSIGHT_NETWORK_PROTOCOL_UDP :
				 				FARSIGHT_NETWORK_PROTOCOL_TCP;
		/* it seems Farsight RTP doesn't handle this correctly right now */
		ti->username = xmlnode_get_attrib(candidate, "ufrag");
		ti->password = xmlnode_get_attrib(candidate, "pwd");
		/* not quite sure about this */
		ti->type = strcmp(type, "host") == 0 ?
					FARSIGHT_CANDIDATE_TYPE_LOCAL :
					strcmp(type, "prflx") == 0 ?
					 FARSIGHT_CANDIDATE_TYPE_DERIVED :
					 FARSIGHT_CANDIDATE_TYPE_RELAY;
			 
		candidates = g_list_append(candidates, ti);
	}
	
	return candidates;
}

FarsightStream *
jabber_jingle_session_get_stream(const JingleSession *sess)
{
	return sess->stream;
}

void
jabber_jingle_session_set_stream(JingleSession *sess, FarsightStream *stream)
{
	sess->stream = stream;
}

PurpleMedia *
jabber_jingle_session_get_media(const JingleSession *sess)
{
	return sess->media;
}

void
jabber_jingle_session_set_media(JingleSession *sess, PurpleMedia *media)
{
	sess->media = media;
}

const char *
jabber_jingle_session_get_remote_jid(const JingleSession *sess)
{
	return sess->remote_jid;
}

void
jabber_jingle_session_set_remote_jid(JingleSession *sess, 
									 const char *remote_jid)
{
	sess->remote_jid = strdup(remote_jid);
}

const char *
jabber_jingle_session_get_initiator(const JingleSession *sess)
{
	return sess->initiator;
}

void
jabber_jingle_session_set_initiator(JingleSession *sess,
									const char *initiator)
{
	sess->initiator = g_strdup(initiator);
}

gboolean
jabber_jingle_session_is_initiator(const JingleSession *sess)
{
	return sess->is_initiator;
}

void
jabber_jingle_session_add_remote_candidate(JingleSession *sess,
										   const GList *candidate)
{
	/* the length of the candidate list should be 1... */
	GList *cand = candidate;
	for (; cand ; cand = cand->next) {
		purple_debug_info("jingle", "Adding remote candidate with id = %s\n",
						  ((FarsightTransportInfo *) cand->data)->candidate_id);
		sess->remote_candidates = g_list_append(sess->remote_candidates, 
												cand->data);
	}
}

static GList *
jabber_jingle_session_get_remote_candidate(const JingleSession *sess,
										   const gchar *id)
{
	GList *candidates = NULL;
	GList *find = sess->remote_candidates;
	for (; find ; find = find->next) {
		const FarsightTransportInfo *candidate =
			(FarsightTransportInfo *) find->data;
		if (!strcmp(candidate->candidate_id, id)) {
			candidates = g_list_append(candidates, (void *) candidate);
		}
	}
	return candidates;
}

static xmlnode *
jabber_jingle_session_create_jingle_element(const JingleSession *sess,
											const char *action)
{
	xmlnode *jingle = xmlnode_new("jingle");
	xmlnode_set_namespace(jingle, "urn:xmpp:tmp:jingle");
	xmlnode_set_attrib(jingle, "action", action);
	xmlnode_set_attrib(jingle, "initiator", 
					   jabber_jingle_session_get_initiator(sess));
	xmlnode_set_attrib(jingle, "responder", 
					   jabber_jingle_session_is_initiator(sess) ?
					   jabber_jingle_session_get_remote_jid(sess) :
					   jabber_jingle_session_get_initiator(sess));
	xmlnode_set_attrib(jingle, "sid", sess->id);
	
	return jingle;
}

xmlnode *
jabber_jingle_session_create_terminate(const JingleSession *sess,
									   const char *reasoncode,
									   const char *reasontext)
{
	xmlnode *jingle = 
		jabber_jingle_session_create_jingle_element(sess, "session-terminate");
	xmlnode_set_attrib(jingle, "resoncode", reasoncode);
	if (reasontext) {
		xmlnode_set_attrib(jingle, "reasontext", reasontext);
	}
	xmlnode_set_attrib(jingle, "sid", sess->id);
	
	return jingle;
}

static xmlnode *
jabber_jingle_session_create_description(const JingleSession *sess)
{
    GList *codecs =
		farsight_stream_get_local_codecs(jabber_jingle_session_get_stream(sess));
    
    xmlnode *description = xmlnode_new("description");
	xmlnode_set_namespace(description, "urn:xmpp:tmp:jingle:apps:audio-rtp");
	
	/* get codecs */
	for (; codecs ; codecs = codecs->next) {
		FarsightCodec *codec = (FarsightCodec*)codecs->data;
		char id[8], clockrate[10];
		xmlnode *payload = xmlnode_new_child(description, "payload-type");
		
		g_snprintf(id, sizeof(id), "%d", codec->id);
		g_snprintf(clockrate, sizeof(clockrate), "%d", codec->clock_rate);
		
		xmlnode_set_attrib(payload, "name", codec->encoding_name);
		xmlnode_set_attrib(payload, "id", id);
		xmlnode_set_attrib(payload, "clockrate", clockrate);
    }
    
    return description;
}
    
/* split into two separate methods, one to generate session-accept
	(includes codecs) and one to generate transport-info (includes transports
	candidates) */
xmlnode *
jabber_jingle_session_create_session_accept(const JingleSession *sess)
{
	xmlnode *jingle = 
		jabber_jingle_session_create_jingle_element(sess, "session-accept");
	xmlnode *content = NULL;
	xmlnode *description = NULL;
	
	
	content = xmlnode_new_child(jingle, "content");
	xmlnode_set_attrib(content, "creator", "initiator");
	xmlnode_set_attrib(content, "name", "audio-content");
	xmlnode_set_attrib(content, "profile", "RTP/AVP");
	
	description = jabber_jingle_session_create_description(sess);
    xmlnode_insert_child(jingle, description);
	
	return jingle;
}

static guint
jabber_jingle_get_priority(guint type, guint network)
{
	switch (type) {
		case FARSIGHT_CANDIDATE_TYPE_LOCAL:
			return network == 0 ? 4096 : network == 1 ? 2048 : 1024;
			break;
		case FARSIGHT_CANDIDATE_TYPE_DERIVED:
			return 126;
			break;
		case FARSIGHT_CANDIDATE_TYPE_RELAY:
			return 100;
			break;
		default:
			return 0; /* unknown type, should not happen */
	}
}

xmlnode *
jabber_jingle_session_create_transport_info(const JingleSession *sess,
											gchar *candidate_id)
{
	xmlnode *jingle = 
		jabber_jingle_session_create_jingle_element(sess, "transport-info");
	xmlnode *content = NULL;
	xmlnode *transport = NULL;
	GList *candidates =
		farsight_stream_get_native_candidate(
			jabber_jingle_session_get_stream(sess), candidate_id);
	
	content = xmlnode_new_child(jingle, "content");
	xmlnode_set_attrib(content, "creator", "initiator");
	xmlnode_set_attrib(content, "name", "audio-content");
	xmlnode_set_attrib(content, "profile", "RTP/AVP");
		
	transport = xmlnode_new_child(content, "transport");
	xmlnode_set_namespace(transport, "urn:xmpp:tmp:jingle:transports:ice-tcp");
	
	/* get transport candidate */
	for (; candidates ; candidates = candidates->next) {
		FarsightTransportInfo *transport_info = 
			(FarsightTransportInfo *) candidates->data;
		char port[8];
		char prio[8];
		char component[8];
		xmlnode *candidate = NULL;
		
		if (!strcmp(transport_info->ip, "127.0.0.1")) {
			continue;
		}
		
		candidate = xmlnode_new_child(transport, "candidate");
		
		g_snprintf(port, sizeof(port), "%d", transport_info->port);
		g_snprintf(prio, sizeof(prio), "%d", 
				   jabber_jingle_get_priority(transport_info->type, 0));
		g_snprintf(component, sizeof(component), "%d", transport_info->component);
		
		xmlnode_set_attrib(candidate, "component", component);
		xmlnode_set_attrib(candidate, "foundation", "1"); /* what about this? */
		xmlnode_set_attrib(candidate, "generation", "0"); /* ? */
		xmlnode_set_attrib(candidate, "ip", transport_info->ip);
		xmlnode_set_attrib(candidate, "network", "0"); /* ? */
		xmlnode_set_attrib(candidate, "port", port);
		xmlnode_set_attrib(candidate, "priority", prio); /* Is this correct? */
		xmlnode_set_attrib(candidate, "protocol",
						   transport_info->proto == FARSIGHT_NETWORK_PROTOCOL_UDP ?
						   "udp" : "tcp");
		if (transport_info->username)
			xmlnode_set_attrib(candidate, "ufrag", transport_info->username);
		if (transport_info->password)
			xmlnode_set_attrib(candidate, "pwd", transport_info->password);
		
		xmlnode_set_attrib(candidate, "type", 
						   transport_info->type == FARSIGHT_CANDIDATE_TYPE_LOCAL ? 
						   "host" :
						   transport_info->type == FARSIGHT_CANDIDATE_TYPE_DERIVED ? 
						   "prflx" :
					       transport_info->type == FARSIGHT_CANDIDATE_TYPE_RELAY ? 
						   "relay" : NULL);
		
	}
	farsight_transport_list_destroy(candidates);
	
	return jingle;
}

xmlnode *
jabber_jingle_session_create_content_replace(const JingleSession *sess,
											 gchar *native_candidate_id,
											 gchar *remote_candidate_id)
{
	xmlnode *jingle = 
		jabber_jingle_session_create_jingle_element(sess, "content-replace");
	xmlnode *content = NULL;
	xmlnode *transport = NULL;
	xmlnode *candidate = NULL;
	xmlnode *description = NULL;
	xmlnode *payload_type = NULL;
	char local_port[8];
	char remote_port[8];
	char prio[8];
	char component[8];
	
	purple_debug_info("jingle", "creating content-modify for native candidate %s " \
					  ", remote candidate %s\n", native_candidate_id,
					  remote_candidate_id);
	
	/* It seems the ID's we get from the Farsight callback is phony... */
	/*
	GList *native_candidates = 
		farsight_stream_get_native_candidate(jabber_jingle_session_get_stream(sess),
											 native_candidate_id);
	purple_debug_info("jingle", "found %d native candidates with id %s\n",
					  g_list_length(native_candidates), native_candidate_id);
	GList *remote_candidates =
		jabber_jingle_session_get_remote_candidate(sess, remote_candidate_id);
	*/
	
	/* we assume lists have length == 1, I think they must... */
	/*
	FarsightTransportInfo *native = 
		(FarsightTransportInfo *) native_candidates->data;
	FarsightTransportInfo *remote =
		(FarsightTransportInfo *) remote_candidates->data;
	*/
	
	content = xmlnode_new_child(jingle, "content");
	xmlnode_set_attrib(content, "creator", "initiator");
	xmlnode_set_attrib(content, "name", "audio-content");
	xmlnode_set_attrib(content, "profile", "RTP/AVP");
	
	description = xmlnode_new_child(content, "description");
	xmlnode_set_namespace(description, "urn:xmpp:tmp:jingle:apps:audio-rtp");
	
	payload_type = xmlnode_new_child(description, "payload-type");
	/* get top codec from codec_intersection to put here... */
	/* later on this should probably handle changing codec */
	
	/* Skip creating the actual "content" element for now, since we don't
		get enough info in the Farsight callback */
#if 0
	transport = xmlnode_new_child(content, "transport");
	xmlnode_set_namespace(transport, "urn:xmpp:tmp:jingle:transports:ice-tcp");
	
	candidate = xmlnode_new_child(transport, "candidate");
	
	g_snprintf(local_port, sizeof(local_port), "%d", native->port);
	g_snprintf(remote_port, sizeof(remote_port), "%d", remote->port);
	g_snprintf(prio, sizeof(prio), "%d",
			   jabber_jingle_get_priority(native->type, 0));
	g_snprintf(component, sizeof(component), "%d", native->component);

	xmlnode_set_attrib(candidate, "component", component);
	xmlnode_set_attrib(candidate, "foundation", "1"); /* what about this? */
	xmlnode_set_attrib(candidate, "generation", "0"); /* ? */
	xmlnode_set_attrib(candidate, "ip", native->ip);
	xmlnode_set_attrib(candidate, "network", "1"); /* ? */
	xmlnode_set_attrib(candidate, "port", local_port);
	xmlnode_set_attrib(candidate, "priority", prio); /* Is this correct? */
	xmlnode_set_attrib(candidate, "protocol", 
					   native->proto == FARSIGHT_NETWORK_PROTOCOL_UDP ? 
					   "udp" : "tcp");
	if (native->username)
		xmlnode_set_attrib(candidate, "ufrag", native->username);
	if (native->password)
		xmlnode_set_attrib(candidate, "pwd", native->password);
		
	xmlnode_set_attrib(candidate, "type", 
					   native->type == FARSIGHT_CANDIDATE_TYPE_LOCAL ? 
					   "host" : 
					   native->type == FARSIGHT_CANDIDATE_TYPE_DERIVED ? 
					   "prflx" : 
					   native->type == FARSIGHT_CANDIDATE_TYPE_RELAY ? 
					   "relay" : NULL);
	/* relay */
	if (native->type == FARSIGHT_CANDIDATE_TYPE_RELAY) {
		/* set rel-addr and rel-port? How? */
	}
	
	xmlnode_set_attrib(candidate, "rem-addr", remote->ip);
	xmlnode_set_attrib(candidate, "rem-port", remote_port);
	
	farsight_transport_list_destroy(native_candidates);
	farsight_transport_list_destroy(remote_candidates);
	
#endif /* 0 */
	
	purple_debug_info("jingle", "End create content modify\n");
	
	return jingle;
}

xmlnode *
jabber_jingle_session_create_content_accept(const JingleSession *sess)
{
	xmlnode *jingle = 
		jabber_jingle_session_create_jingle_element(sess, "content-accept");
	
    xmlnode *content = xmlnode_new_child(jingle, "content");
	xmlnode *description = jabber_jingle_session_create_description(sess);
    
    xmlnode_set_attrib(content, "creator", "initiator");
	xmlnode_set_attrib(content, "name", "audio-content");
	xmlnode_set_attrib(content, "profile", "RTP/AVP");
	
    xmlnode_insert_child(jingle, description);
    
	return jingle;
}

void
jabber_jingle_session_set_remote_codecs(JingleSession *sess, GList *codecs)
{
    if (sess->remote_codecs) {
        farsight_codec_list_destroy(sess->remote_codecs);
    }
    sess->remote_codecs = codecs;
}

GList *
jabber_jingle_session_get_remote_codecs(const JingleSession *sess)
{
    return sess->remote_codecs;
}

#endif /* USE_FARSIGHT */
