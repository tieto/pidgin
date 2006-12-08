/**
 * @file jabber.h
 *
 * gaim
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _GAIM_JABBER_H_
#define _GAIM_JABBER_H_

#include <libxml/parser.h>
#include <glib.h>
#include "circbuffer.h"
#include "connection.h"
#include "dnssrv.h"
#include "roomlist.h"
#include "sslconn.h"

#include "jutil.h"
#include "xmlnode.h"

#ifdef HAVE_CYRUS_SASL
#include <sasl/sasl.h>
#endif

#define CAPS0115_NODE "http://gaim.sf.net/caps"

typedef enum {
	JABBER_CAP_NONE           = 0,
	JABBER_CAP_XHTML          = 1 << 0,
	JABBER_CAP_COMPOSING      = 1 << 1,
	JABBER_CAP_SI             = 1 << 2,
	JABBER_CAP_SI_FILE_XFER   = 1 << 3,
	JABBER_CAP_BYTESTREAMS    = 1 << 4,
	JABBER_CAP_IBB            = 1 << 5,
	JABBER_CAP_CHAT_STATES    = 1 << 6,
	JABBER_CAP_IQ_SEARCH      = 1 << 7,
	JABBER_CAP_IQ_REGISTER    = 1 << 8,
	JABBER_CAP_RETRIEVED      = 1 << 31
} JabberCapabilities;

typedef enum {
	JABBER_STREAM_OFFLINE,
	JABBER_STREAM_CONNECTING,
	JABBER_STREAM_INITIALIZING,
	JABBER_STREAM_AUTHENTICATING,
	JABBER_STREAM_REINITIALIZING,
	JABBER_STREAM_CONNECTED
} JabberStreamState;

typedef struct _JabberStream
{
	int fd;

	GaimSrvQueryData *srv_query_data;

	xmlParserCtxt *context;
	xmlnode *current;

	enum {
		JABBER_PROTO_0_9,
		JABBER_PROTO_1_0
	} protocol_version;
	enum {
		JABBER_AUTH_UNKNOWN,
		JABBER_AUTH_DIGEST_MD5,
		JABBER_AUTH_PLAIN,
		JABBER_AUTH_IQ_AUTH,
		JABBER_AUTH_CYRUS
	} auth_type;
	char *stream_id;
	JabberStreamState state;

	/* SASL authentication */
	char *expected_rspauth;

	GHashTable *buddies;
	gboolean roster_parsed;

	GHashTable *chats;
	GList *chat_servers;
	GaimRoomlist *roomlist;
	GList *user_directories;

	GHashTable *iq_callbacks;
	GHashTable *disco_callbacks;
	int next_id;


	GList *oob_file_transfers;
	GList *file_transfers;

	time_t idle;

	JabberID *user;
	GaimConnection *gc;
	GaimSslConnection *gsc;

	gboolean registration;

	char *avatar_hash;
	GSList *pending_avatar_requests;

	GaimCircBuffer *write_buffer;
	guint writeh;

	gboolean reinit;

	gboolean googletalk;
	char *server_name;

	/* OK, this stays at the end of the struct, so plugins can depend
	 * on the rest of the stuff being in the right place
	 */
#ifdef HAVE_CYRUS_SASL
	sasl_conn_t *sasl;
	sasl_callback_t *sasl_cb;
	int sasl_state;
	int sasl_maxbuf;
	GString *sasl_mechs;
#endif

} JabberStream;

void jabber_process_packet(JabberStream *js, xmlnode *packet);
void jabber_send(JabberStream *js, xmlnode *data);
void jabber_send_raw(JabberStream *js, const char *data, int len);

void jabber_stream_set_state(JabberStream *js, JabberStreamState state);

void jabber_register_parse(JabberStream *js, xmlnode *packet);
void jabber_register_start(JabberStream *js);

char *jabber_get_next_id(JabberStream *js);

char *jabber_parse_error(JabberStream *js, xmlnode *packet);

#endif /* _GAIM_JABBER_H_ */
