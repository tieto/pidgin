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

#include "internal.h"
#include "glibcompat.h"

#include "backend-fs2.h"

#ifdef USE_VV
#include "backend-iface.h"
#include "debug.h"
#include "network.h"
#include "media-gst.h"

#include <farstream/fs-conference.h>
#include <farstream/fs-element-added-notifier.h>
#include <farstream/fs-utils.h>
#include <gst/gststructure.h>

#if !GST_CHECK_VERSION(1,0,0)
#define gst_registry_get() gst_registry_get_default()
#endif

/** @copydoc _PurpleMediaBackendFs2Class */
typedef struct _PurpleMediaBackendFs2Class PurpleMediaBackendFs2Class;
/** @copydoc _PurpleMediaBackendFs2Private */
typedef struct _PurpleMediaBackendFs2Private PurpleMediaBackendFs2Private;
/** @copydoc _PurpleMediaBackendFs2Session */
typedef struct _PurpleMediaBackendFs2Session PurpleMediaBackendFs2Session;
/** @copydoc _PurpleMediaBackendFs2Stream */
typedef struct _PurpleMediaBackendFs2Stream PurpleMediaBackendFs2Stream;

#define PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(obj) \
		(G_TYPE_INSTANCE_GET_PRIVATE((obj), \
		PURPLE_TYPE_MEDIA_BACKEND_FS2, PurpleMediaBackendFs2Private))

static void purple_media_backend_iface_init(PurpleMediaBackendIface *iface);

static gboolean
gst_bus_cb(GstBus *bus, GstMessage *msg, PurpleMediaBackendFs2 *self);
static void
state_changed_cb(PurpleMedia *media, PurpleMediaState state,
		gchar *sid, gchar *name, PurpleMediaBackendFs2 *self);
static void
stream_info_cb(PurpleMedia *media, PurpleMediaInfoType type,
		gchar *sid, gchar *name, gboolean local,
		PurpleMediaBackendFs2 *self);

static gboolean purple_media_backend_fs2_add_stream(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *who,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter,
		guint num_params, GParameter *params);
static void purple_media_backend_fs2_add_remote_candidates(
		PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *remote_candidates);
static gboolean purple_media_backend_fs2_codecs_ready(PurpleMediaBackend *self,
		const gchar *sess_id);
static GList *purple_media_backend_fs2_get_codecs(PurpleMediaBackend *self,
		const gchar *sess_id);
static GList *purple_media_backend_fs2_get_local_candidates(
		PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant);
static gboolean purple_media_backend_fs2_set_encryption_parameters (
		PurpleMediaBackend *self, const gchar *sess_id, const gchar *cipher,
		const gchar *auth, const gchar *key, gsize key_len);
static gboolean purple_media_backend_fs2_set_decryption_parameters(
		PurpleMediaBackend *self, const gchar *sess_id,
		const gchar *participant, const gchar *cipher,
		const gchar *auth, const gchar *key, gsize key_len);
static gboolean purple_media_backend_fs2_set_remote_codecs(
		PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *codecs);
static gboolean purple_media_backend_fs2_set_send_codec(
		PurpleMediaBackend *self, const gchar *sess_id,
		PurpleMediaCodec *codec);
static void purple_media_backend_fs2_set_params(PurpleMediaBackend *self,
		guint num_params, GParameter *params);
static const gchar **purple_media_backend_fs2_get_available_params(void);
static gboolean purple_media_backend_fs2_send_dtmf(
		PurpleMediaBackend *self, const gchar *sess_id,
		gchar dtmf, guint8 volume, guint16 duration);
static gboolean purple_media_backend_fs2_set_send_rtcp_mux(
		PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		gboolean send_rtcp_mux);

static void remove_element(GstElement *element);

static void free_stream(PurpleMediaBackendFs2Stream *stream);
static void free_session(PurpleMediaBackendFs2Session *session);

struct _PurpleMediaBackendFs2Class
{
	GObjectClass parent_class;
};

struct _PurpleMediaBackendFs2
{
	GObject parent;
};

G_DEFINE_TYPE_WITH_CODE(PurpleMediaBackendFs2, purple_media_backend_fs2,
		G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(
		PURPLE_TYPE_MEDIA_BACKEND, purple_media_backend_iface_init));

struct _PurpleMediaBackendFs2Stream
{
	PurpleMediaBackendFs2Session *session;
	gchar *participant;
	FsStream *stream;

	gboolean supports_add;

	GstElement *src;
	GstElement *tee;
	GstElement *volume;
	GstElement *level;
	GstElement *fakesink;
	GstElement *queue;

	GList *local_candidates;
	GList *remote_candidates;

	guint connected_cb_id;
};

struct _PurpleMediaBackendFs2Session
{
	PurpleMediaBackendFs2 *backend;
	gchar *id;
	FsSession *session;

	GstElement *src;
	GstElement *tee;
	GstElement *srcvalve;

	GstPad *srcpad;

	PurpleMediaSessionType type;
};

struct _PurpleMediaBackendFs2Private
{
	PurpleMedia *media;
	GstElement *confbin;
	FsConference *conference;
	gchar *conference_type;

	FsElementAddedNotifier *notifier;

	GHashTable *sessions;
	GHashTable *participants;

	GList *streams;

	gdouble silence_threshold;
};

enum {
	PROP_0,
	PROP_CONFERENCE_TYPE,
	PROP_MEDIA,
};

static void
purple_media_backend_fs2_init(PurpleMediaBackendFs2 *self)
{
#if GLIB_CHECK_VERSION(2, 37, 3)
	/* silence a warning */
	(void)purple_media_backend_fs2_get_instance_private;
#endif
}

static FsCandidateType
purple_media_candidate_type_to_fs(PurpleMediaCandidateType type)
{
	switch (type) {
		case PURPLE_MEDIA_CANDIDATE_TYPE_HOST:
			return FS_CANDIDATE_TYPE_HOST;
		case PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX:
			return FS_CANDIDATE_TYPE_SRFLX;
		case PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX:
			return FS_CANDIDATE_TYPE_PRFLX;
		case PURPLE_MEDIA_CANDIDATE_TYPE_RELAY:
			return FS_CANDIDATE_TYPE_RELAY;
		case PURPLE_MEDIA_CANDIDATE_TYPE_MULTICAST:
			return FS_CANDIDATE_TYPE_MULTICAST;
	}
	g_return_val_if_reached(FS_CANDIDATE_TYPE_HOST);
}

static PurpleMediaCandidateType
purple_media_candidate_type_from_fs(FsCandidateType type)
{
	switch (type) {
		case FS_CANDIDATE_TYPE_HOST:
			return PURPLE_MEDIA_CANDIDATE_TYPE_HOST;
		case FS_CANDIDATE_TYPE_SRFLX:
			return PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX;
		case FS_CANDIDATE_TYPE_PRFLX:
			return PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX;
		case FS_CANDIDATE_TYPE_RELAY:
			return PURPLE_MEDIA_CANDIDATE_TYPE_RELAY;
		case FS_CANDIDATE_TYPE_MULTICAST:
			return PURPLE_MEDIA_CANDIDATE_TYPE_MULTICAST;
	}
	g_return_val_if_reached(PURPLE_MEDIA_CANDIDATE_TYPE_HOST);
}

static FsNetworkProtocol
purple_media_network_protocol_to_fs(PurpleMediaNetworkProtocol protocol)
{
	switch (protocol) {
		case PURPLE_MEDIA_NETWORK_PROTOCOL_UDP:
			return FS_NETWORK_PROTOCOL_UDP;
		case PURPLE_MEDIA_NETWORK_PROTOCOL_TCP_PASSIVE:
			return FS_NETWORK_PROTOCOL_TCP_PASSIVE;
		case PURPLE_MEDIA_NETWORK_PROTOCOL_TCP_ACTIVE:
			return FS_NETWORK_PROTOCOL_TCP_ACTIVE;
		case PURPLE_MEDIA_NETWORK_PROTOCOL_TCP_SO:
			return FS_NETWORK_PROTOCOL_TCP_SO;
		default:
			g_return_val_if_reached(FS_NETWORK_PROTOCOL_TCP);
	}
}

static PurpleMediaNetworkProtocol
purple_media_network_protocol_from_fs(FsNetworkProtocol protocol)
{
	switch (protocol) {
		case FS_NETWORK_PROTOCOL_UDP:
			return PURPLE_MEDIA_NETWORK_PROTOCOL_UDP;
		case FS_NETWORK_PROTOCOL_TCP_PASSIVE:
			return PURPLE_MEDIA_NETWORK_PROTOCOL_TCP_PASSIVE;
		case FS_NETWORK_PROTOCOL_TCP_ACTIVE:
			return PURPLE_MEDIA_NETWORK_PROTOCOL_TCP_ACTIVE;
		case FS_NETWORK_PROTOCOL_TCP_SO:
			return PURPLE_MEDIA_NETWORK_PROTOCOL_TCP_SO;
		default:
			g_return_val_if_reached(PURPLE_MEDIA_NETWORK_PROTOCOL_TCP_PASSIVE);
	}
}

static GstPadProbeReturn
event_probe_cb(GstPad *srcpad, GstPadProbeInfo *info, gpointer unused)
{
	GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
	if (GST_EVENT_TYPE(event) == GST_EVENT_CUSTOM_DOWNSTREAM
		&& gst_event_has_name(event, "purple-unlink-tee")) {

		const GstStructure *s = gst_event_get_structure(event);

		gst_pad_unlink(srcpad, gst_pad_get_peer(srcpad));

		gst_pad_remove_probe(srcpad,
			g_value_get_ulong(gst_structure_get_value(s, "handler-id")));

		if (g_value_get_boolean(gst_structure_get_value(s, "release-pad")))
			gst_element_release_request_pad(GST_ELEMENT_PARENT(srcpad), srcpad);

		return GST_PAD_PROBE_DROP;
	}

	return GST_PAD_PROBE_OK;
}

static void
unlink_teepad_dynamic(GstPad *srcpad, gboolean release_pad)
{
	gulong id = gst_pad_add_probe(srcpad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
	                              event_probe_cb, NULL, NULL);

	if (GST_IS_GHOST_PAD(srcpad))
		srcpad = gst_ghost_pad_get_target(GST_GHOST_PAD(srcpad));

	gst_element_send_event(gst_pad_get_parent_element(srcpad),
		gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM,
			gst_structure_new("purple-unlink-tee",
				"release-pad", G_TYPE_BOOLEAN, release_pad,
				"handler-id", G_TYPE_ULONG, id,
				NULL)));
}

static void
purple_media_backend_fs2_dispose(GObject *obj)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(obj);
	GList *iter = NULL;

	purple_debug_info("backend-fs2", "purple_media_backend_fs2_dispose\n");

	if (priv->notifier) {
		g_object_unref(priv->notifier);
		priv->notifier = NULL;
	}

	if (priv->confbin) {
		GstElement *pipeline;

		pipeline = purple_media_manager_get_pipeline(
				purple_media_get_manager(priv->media));

		/* All connections to media sources should be blocked before confbin is
		 * removed, to prevent freezing of any other simultaneously running
		 * media calls. */
		if (priv->sessions) {
			GList *sessions = g_hash_table_get_values(priv->sessions);
			for (; sessions; sessions =
					g_list_delete_link(sessions, sessions)) {
				PurpleMediaBackendFs2Session *session = sessions->data;
				if (session->srcpad) {
					unlink_teepad_dynamic(session->srcpad, FALSE);
					gst_object_unref(session->srcpad);
					session->srcpad = NULL;
				}
			}
		}

		gst_element_set_locked_state(priv->confbin, TRUE);
		gst_element_set_state(GST_ELEMENT(priv->confbin),
				GST_STATE_NULL);

		if (pipeline) {
			GstBus *bus;
			gst_bin_remove(GST_BIN(pipeline), priv->confbin);
			bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
			g_signal_handlers_disconnect_matched(G_OBJECT(bus),
					G_SIGNAL_MATCH_FUNC |
					G_SIGNAL_MATCH_DATA,
					0, 0, 0, gst_bus_cb, obj);
			gst_object_unref(bus);
		} else {
			purple_debug_warning("backend-fs2", "Unable to "
					"properly dispose the conference. "
					"Couldn't get the pipeline.\n");
		}

		priv->confbin = NULL;
		priv->conference = NULL;

	}

	if (priv->sessions) {
		GList *sessions = g_hash_table_get_values(priv->sessions);

		for (; sessions; sessions =
				g_list_delete_link(sessions, sessions)) {
			PurpleMediaBackendFs2Session *session =
					sessions->data;

			if (session->session) {
				g_object_unref(session->session);
				session->session = NULL;
			}
		}
	}

	if (priv->participants) {
		g_hash_table_destroy(priv->participants);
		priv->participants = NULL;
	}

	for (iter = priv->streams; iter; iter = g_list_next(iter)) {
		PurpleMediaBackendFs2Stream *stream = iter->data;
		if (stream->stream) {
			g_object_unref(stream->stream);
			stream->stream = NULL;
		}
	}

	if (priv->media) {
		g_object_remove_weak_pointer(G_OBJECT(priv->media),
				(gpointer*)&priv->media);
		priv->media = NULL;
	}

	G_OBJECT_CLASS(purple_media_backend_fs2_parent_class)->dispose(obj);
}

static void
purple_media_backend_fs2_finalize(GObject *obj)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(obj);

	purple_debug_info("backend-fs2", "purple_media_backend_fs2_finalize\n");

	g_free(priv->conference_type);

	for (; priv->streams; priv->streams =
			g_list_delete_link(priv->streams, priv->streams)) {
		PurpleMediaBackendFs2Stream *stream = priv->streams->data;
		free_stream(stream);
	}

	if (priv->sessions) {
		GList *sessions = g_hash_table_get_values(priv->sessions);

		for (; sessions; sessions =
				g_list_delete_link(sessions, sessions)) {
			PurpleMediaBackendFs2Session *session =
					sessions->data;
			free_session(session);
		}

		g_hash_table_destroy(priv->sessions);
	}

	G_OBJECT_CLASS(purple_media_backend_fs2_parent_class)->finalize(obj);
}

static void
purple_media_backend_fs2_set_property(GObject *object, guint prop_id,
		const GValue *value, GParamSpec *pspec)
{
	PurpleMediaBackendFs2Private *priv;
	g_return_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(object));

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(object);

	switch (prop_id) {
		case PROP_CONFERENCE_TYPE:
			priv->conference_type = g_value_dup_string(value);
			break;
		case PROP_MEDIA:
			priv->media = g_value_get_object(value);

			if (priv->media == NULL)
				break;

			g_object_add_weak_pointer(G_OBJECT(priv->media),
					(gpointer*)&priv->media);

			g_signal_connect(G_OBJECT(priv->media),
					"state-changed",
					G_CALLBACK(state_changed_cb),
					PURPLE_MEDIA_BACKEND_FS2(object));
			g_signal_connect(G_OBJECT(priv->media), "stream-info",
					G_CALLBACK(stream_info_cb),
					PURPLE_MEDIA_BACKEND_FS2(object));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(
					object, prop_id, pspec);
			break;
	}
}

static void
purple_media_backend_fs2_get_property(GObject *object, guint prop_id,
		GValue *value, GParamSpec *pspec)
{
	PurpleMediaBackendFs2Private *priv;
	g_return_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(object));

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(object);

	switch (prop_id) {
		case PROP_CONFERENCE_TYPE:
			g_value_set_string(value, priv->conference_type);
			break;
		case PROP_MEDIA:
			g_value_set_object(value, priv->media);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(
					object, prop_id, pspec);
			break;
	}
}

static void
purple_media_backend_fs2_class_init(PurpleMediaBackendFs2Class *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	GList *features;
	GList *it;

	gobject_class->dispose = purple_media_backend_fs2_dispose;
	gobject_class->finalize = purple_media_backend_fs2_finalize;
	gobject_class->set_property = purple_media_backend_fs2_set_property;
	gobject_class->get_property = purple_media_backend_fs2_get_property;

	g_object_class_override_property(gobject_class, PROP_CONFERENCE_TYPE,
			"conference-type");
	g_object_class_override_property(gobject_class, PROP_MEDIA, "media");

	g_type_class_add_private(klass, sizeof(PurpleMediaBackendFs2Private));

	/* VA-API elements aren't well supported in Farstream. Ignore them. */
	features = gst_registry_get_feature_list_by_plugin(gst_registry_get(),
			"vaapi");
	for (it = features; it; it = it->next) {
		gst_plugin_feature_set_rank((GstPluginFeature *)it->data,
				GST_RANK_NONE);
	}
	gst_plugin_feature_list_free(features);
}

static void
purple_media_backend_iface_init(PurpleMediaBackendIface *iface)
{
	iface->add_stream = purple_media_backend_fs2_add_stream;
	iface->add_remote_candidates =
			purple_media_backend_fs2_add_remote_candidates;
	iface->codecs_ready = purple_media_backend_fs2_codecs_ready;
	iface->get_codecs = purple_media_backend_fs2_get_codecs;
	iface->get_local_candidates =
			purple_media_backend_fs2_get_local_candidates;
	iface->set_remote_codecs = purple_media_backend_fs2_set_remote_codecs;
	iface->set_send_codec = purple_media_backend_fs2_set_send_codec;
	iface->set_encryption_parameters =
			purple_media_backend_fs2_set_encryption_parameters;
	iface->set_decryption_parameters =
			purple_media_backend_fs2_set_decryption_parameters;
	iface->set_params = purple_media_backend_fs2_set_params;
	iface->get_available_params = purple_media_backend_fs2_get_available_params;
	iface->send_dtmf = purple_media_backend_fs2_send_dtmf;
	iface->set_send_rtcp_mux = purple_media_backend_fs2_set_send_rtcp_mux;
}

static FsMediaType
session_type_to_fs_media_type(PurpleMediaSessionType type)
{
	if (type & PURPLE_MEDIA_AUDIO)
		return FS_MEDIA_TYPE_AUDIO;
	else if (type & PURPLE_MEDIA_VIDEO)
		return FS_MEDIA_TYPE_VIDEO;
#ifdef HAVE_MEDIA_APPLICATION
	else if (type & PURPLE_MEDIA_APPLICATION)
		return FS_MEDIA_TYPE_APPLICATION;
#endif
	else
		return 0;
}

static FsStreamDirection
session_type_to_fs_stream_direction(PurpleMediaSessionType type)
{
	if ((type & PURPLE_MEDIA_AUDIO) == PURPLE_MEDIA_AUDIO ||
		(type & PURPLE_MEDIA_VIDEO) == PURPLE_MEDIA_VIDEO)
		return FS_DIRECTION_BOTH;
	else if ((type & PURPLE_MEDIA_SEND_AUDIO) ||
			(type & PURPLE_MEDIA_SEND_VIDEO))
		return FS_DIRECTION_SEND;
	else if ((type & PURPLE_MEDIA_RECV_AUDIO) ||
			(type & PURPLE_MEDIA_RECV_VIDEO))
		return FS_DIRECTION_RECV;
#ifdef HAVE_MEDIA_APPLICATION
	else if ((type & PURPLE_MEDIA_APPLICATION) == PURPLE_MEDIA_APPLICATION)
		return FS_DIRECTION_BOTH;
	else if (type & PURPLE_MEDIA_SEND_APPLICATION)
		return FS_DIRECTION_SEND;
	else if (type & PURPLE_MEDIA_RECV_APPLICATION)
		return FS_DIRECTION_RECV;
#endif
	else
		return FS_DIRECTION_NONE;
}

static PurpleMediaSessionType
session_type_from_fs(FsMediaType type, FsStreamDirection direction)
{
	PurpleMediaSessionType result = PURPLE_MEDIA_NONE;
	if (type == FS_MEDIA_TYPE_AUDIO) {
		if (direction & FS_DIRECTION_SEND)
			result |= PURPLE_MEDIA_SEND_AUDIO;
		if (direction & FS_DIRECTION_RECV)
			result |= PURPLE_MEDIA_RECV_AUDIO;
	} else if (type == FS_MEDIA_TYPE_VIDEO) {
		if (direction & FS_DIRECTION_SEND)
			result |= PURPLE_MEDIA_SEND_VIDEO;
		if (direction & FS_DIRECTION_RECV)
			result |= PURPLE_MEDIA_RECV_VIDEO;
#ifdef HAVE_MEDIA_APPLICATION
	} else if (type == FS_MEDIA_TYPE_APPLICATION) {
		if (direction & FS_DIRECTION_SEND)
			result |= PURPLE_MEDIA_SEND_APPLICATION;
		if (direction & FS_DIRECTION_RECV)
			result |= PURPLE_MEDIA_RECV_APPLICATION;
#endif
	}
	return result;
}

static FsCandidate *
candidate_to_fs(PurpleMediaCandidate *candidate)
{
	FsCandidate *fscandidate;
	gchar *foundation;
	guint component_id;
	gchar *ip;
	guint port;
	gchar *base_ip;
	guint base_port;
	PurpleMediaNetworkProtocol proto;
	guint32 priority;
	PurpleMediaCandidateType type;
	gchar *username;
	gchar *password;
	guint ttl;

	if (candidate == NULL)
		return NULL;

	g_object_get(G_OBJECT(candidate),
			"foundation", &foundation,
			"component-id", &component_id,
			"ip", &ip,
			"port", &port,
			"base-ip", &base_ip,
			"base-port", &base_port,
			"protocol", &proto,
			"priority", &priority,
			"type", &type,
			"username", &username,
			"password", &password,
			"ttl", &ttl,
			NULL);

	fscandidate = fs_candidate_new(foundation,
			component_id, purple_media_candidate_type_to_fs(type),
			purple_media_network_protocol_to_fs(proto), ip, port);

	fscandidate->base_ip = base_ip;
	fscandidate->base_port = base_port;
	fscandidate->priority = priority;
	fscandidate->username = username;
	fscandidate->password = password;
	fscandidate->ttl = ttl;

	g_free(foundation);
	g_free(ip);
	return fscandidate;
}

static GList *
candidate_list_to_fs(GList *candidates)
{
	GList *new_list = NULL;

	for (; candidates; candidates = g_list_next(candidates)) {
		new_list = g_list_prepend(new_list,
				candidate_to_fs(candidates->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

static PurpleMediaCandidate *
candidate_from_fs(FsCandidate *fscandidate)
{
	PurpleMediaCandidate *candidate;

	if (fscandidate == NULL)
		return NULL;

	candidate = purple_media_candidate_new(fscandidate->foundation,
		fscandidate->component_id,
		purple_media_candidate_type_from_fs(fscandidate->type),
		purple_media_network_protocol_from_fs(fscandidate->proto),
		fscandidate->ip, fscandidate->port);
	g_object_set(candidate,
			"base-ip", fscandidate->base_ip,
			"base-port", fscandidate->base_port,
			"priority", fscandidate->priority,
			"username", fscandidate->username,
			"password", fscandidate->password,
			"ttl", fscandidate->ttl, NULL);
	return candidate;
}

static GList *
candidate_list_from_fs(GList *candidates)
{
	GList *new_list = NULL;

	for (; candidates; candidates = g_list_next(candidates)) {
		new_list = g_list_prepend(new_list,
			candidate_from_fs(candidates->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

static FsCodec *
codec_to_fs(const PurpleMediaCodec *codec)
{
	FsCodec *new_codec;
	gint id;
	char *encoding_name;
	PurpleMediaSessionType media_type;
	guint clock_rate;
	guint channels;
	GList *iter;

	if (codec == NULL)
		return NULL;

	g_object_get(G_OBJECT(codec),
			"id", &id,
			"encoding-name", &encoding_name,
			"media-type", &media_type,
			"clock-rate", &clock_rate,
			"channels", &channels,
			"optional-params", &iter,
			NULL);

	new_codec = fs_codec_new(id, encoding_name,
			session_type_to_fs_media_type(media_type),
			clock_rate);
	new_codec->channels = channels;

	for (; iter; iter = g_list_next(iter)) {
		PurpleKeyValuePair *param = (PurpleKeyValuePair*)iter->data;
		fs_codec_add_optional_parameter(new_codec,
				param->key, param->value);
	}

	g_free(encoding_name);
	return new_codec;
}

static PurpleMediaCodec *
codec_from_fs(const FsCodec *codec)
{
	PurpleMediaCodec *new_codec;
	GList *iter;

	if (codec == NULL)
		return NULL;

	new_codec = purple_media_codec_new(codec->id, codec->encoding_name,
			session_type_from_fs(codec->media_type,
			FS_DIRECTION_BOTH), codec->clock_rate);
	g_object_set(new_codec, "channels", codec->channels, NULL);

	for (iter = codec->optional_params; iter; iter = g_list_next(iter)) {
		FsCodecParameter *param = (FsCodecParameter*)iter->data;
		purple_media_codec_add_optional_parameter(new_codec,
				param->name, param->value);
	}

	return new_codec;
}

static GList *
codec_list_from_fs(GList *codecs)
{
	GList *new_list = NULL;

	for (; codecs; codecs = g_list_next(codecs)) {
		new_list = g_list_prepend(new_list,
				codec_from_fs(codecs->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

static GList *
codec_list_to_fs(GList *codecs)
{
	GList *new_list = NULL;

	for (; codecs; codecs = g_list_next(codecs)) {
		new_list = g_list_prepend(new_list,
				codec_to_fs(codecs->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

static PurpleMediaBackendFs2Session *
get_session(PurpleMediaBackendFs2 *self, const gchar *sess_id)
{
	PurpleMediaBackendFs2Private *priv;
	PurpleMediaBackendFs2Session *session = NULL;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), NULL);

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);

	if (priv->sessions != NULL)
		session = g_hash_table_lookup(priv->sessions, sess_id);

	return session;
}

static FsParticipant *
get_participant(PurpleMediaBackendFs2 *self, const gchar *name)
{
	PurpleMediaBackendFs2Private *priv;
	FsParticipant *participant = NULL;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), NULL);

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);

	if (priv->participants != NULL)
		participant = g_hash_table_lookup(priv->participants, name);

	return participant;
}

static PurpleMediaBackendFs2Stream *
get_stream(PurpleMediaBackendFs2 *self,
		const gchar *sess_id, const gchar *name)
{
	PurpleMediaBackendFs2Private *priv;
	GList *streams;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), NULL);

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	streams = priv->streams;

	for (; streams; streams = g_list_next(streams)) {
		PurpleMediaBackendFs2Stream *stream = streams->data;
		if (!strcmp(stream->session->id, sess_id) &&
				!strcmp(stream->participant, name))
			return stream;
	}

	return NULL;
}

static GList *
get_streams(PurpleMediaBackendFs2 *self,
		const gchar *sess_id, const gchar *name)
{
	PurpleMediaBackendFs2Private *priv;
	GList *streams, *ret = NULL;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), NULL);

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	streams = priv->streams;

	for (; streams; streams = g_list_next(streams)) {
		PurpleMediaBackendFs2Stream *stream = streams->data;

		if (sess_id != NULL && strcmp(stream->session->id, sess_id))
			continue;
		else if (name != NULL && strcmp(stream->participant, name))
			continue;
		else
			ret = g_list_prepend(ret, stream);
	}

	ret = g_list_reverse(ret);
	return ret;
}

static PurpleMediaBackendFs2Session *
get_session_from_fs_stream(PurpleMediaBackendFs2 *self, FsStream *stream)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	FsSession *fssession;
	GList *values;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), NULL);
	g_return_val_if_fail(FS_IS_STREAM(stream), NULL);

	g_object_get(stream, "session", &fssession, NULL);

	values = g_hash_table_get_values(priv->sessions);

	for (; values; values = g_list_delete_link(values, values)) {
		PurpleMediaBackendFs2Session *session = values->data;

		if (session->session == fssession) {
			g_list_free(values);
			g_object_unref(fssession);
			return session;
		}
	}

	g_object_unref(fssession);
	return NULL;
}

static gdouble
gst_msg_db_to_percent(GstMessage *msg, gchar *value_name)
{
	const GValue *list;
	const GValue *value;
	gdouble value_db;
	gdouble percent;

	list = gst_structure_get_value(gst_message_get_structure(msg), value_name);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	value = g_value_array_get_nth(g_value_get_boxed(list), 0);
G_GNUC_END_IGNORE_DEPRECATIONS
	value_db = g_value_get_double(value);
	percent = pow(10, value_db / 20);
	return (percent > 1.0) ? 1.0 : percent;
}

static void
purple_media_error_fs(PurpleMedia *media, const gchar *error,
		const GstStructure *fs_error)
{
	const gchar *error_msg = gst_structure_get_string(fs_error, "error-msg");

	purple_media_error(media, "%s%s%s", error,
	                   error_msg ? _("\n\nMessage from Farstream: ") : "",
	                   error_msg ? error_msg : "");
}

static void
gst_handle_message_element(GstBus *bus, GstMessage *msg,
		PurpleMediaBackendFs2 *self)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	GstElement *src = GST_ELEMENT(GST_MESSAGE_SRC(msg));
	static guint level_id = 0;
	const GstStructure *structure = gst_message_get_structure(msg);

	if (level_id == 0)
		level_id = g_signal_lookup("level", PURPLE_TYPE_MEDIA);

	if (gst_structure_has_name(structure, "level")) {
		GstElement *src = GST_ELEMENT(GST_MESSAGE_SRC(msg));
		gchar *name;
		gchar *participant = NULL;
		PurpleMediaBackendFs2Session *session = NULL;
		gdouble percent;

		if (!PURPLE_IS_MEDIA(priv->media) ||
				GST_ELEMENT_PARENT(src) != priv->confbin)
			return;

		name = gst_element_get_name(src);

		if (!strncmp(name, "sendlevel_", 10)) {
			session = get_session(self, name+10);
			if (priv->silence_threshold > 0) {
				percent = gst_msg_db_to_percent(msg, "decay");
				g_object_set(session->srcvalve,
						"drop", (percent < priv->silence_threshold), NULL);
			}
		}

		g_free(name);

		if (!g_signal_has_handler_pending(priv->media, level_id, 0, FALSE))
			return;

		if (!session) {
			GList *iter = priv->streams;
			PurpleMediaBackendFs2Stream *stream;
			for (; iter; iter = g_list_next(iter)) {
				stream = iter->data;
				if (stream->level == src) {
					session = stream->session;
					participant = stream->participant;
					break;
				}
			}
		}

		if (!session)
			return;

		percent = gst_msg_db_to_percent(msg, "rms");

		g_signal_emit(priv->media, level_id, 0,
				session->id, participant, percent);
		return;
	}

	if (!FS_IS_CONFERENCE(src) || !PURPLE_IS_MEDIA_BACKEND(self) ||
			priv->conference != FS_CONFERENCE(src))
		return;

	if (gst_structure_has_name(structure, "farstream-error")) {
		FsError error_no;
		gboolean error_emitted = FALSE;
		gst_structure_get_enum(structure, "error-no",
				FS_TYPE_ERROR, (gint*)&error_no);
		switch (error_no) {
			case FS_ERROR_CONSTRUCTION:
				purple_media_error_fs(priv->media,
				                      _("Error initializing the call. "
				                        "This probably denotes problem in "
				                        "installation of GStreamer or Farstream."),
				                      structure);
				error_emitted = TRUE;
				break;
			case FS_ERROR_NETWORK:
				purple_media_error_fs(priv->media, _("Network error."),
				                      structure);
				error_emitted = TRUE;
				purple_media_end(priv->media, NULL, NULL);
				break;
			case FS_ERROR_NEGOTIATION_FAILED:
				purple_media_error_fs(priv->media,
				                      _("Codec negotiation failed. "
				                        "This problem might be resolved by installing "
				                        "more GStreamer codecs."),
				                      structure);
				error_emitted = TRUE;
				purple_media_end(priv->media, NULL, NULL);
				break;
			case FS_ERROR_NO_CODECS:
				purple_media_error(priv->media,
				                   _("No codecs found. "
				                     "Install some GStreamer codecs found "
				                     "in GStreamer plugins packages."));
				error_emitted = TRUE;
				purple_media_end(priv->media, NULL, NULL);
				break;
			default:
				purple_debug_error("backend-fs2",
						"farstream-error: %i: %s\n",
						error_no,
						gst_structure_get_string(structure, "error-msg"));
				break;
		}

		if (FS_ERROR_IS_FATAL(error_no)) {
			if (!error_emitted)
			purple_media_error(priv->media,
			                   _("A non-recoverable Farstream error has occurred."));
			purple_media_end(priv->media, NULL, NULL);
		}
	} else if (gst_structure_has_name(structure,
			"farstream-new-local-candidate")) {
		const GValue *value;
		FsStream *stream;
		FsCandidate *local_candidate;
		PurpleMediaCandidate *candidate;
		FsParticipant *participant;
		PurpleMediaBackendFs2Session *session;
		PurpleMediaBackendFs2Stream *media_stream;
		gchar *name;

		value = gst_structure_get_value(structure, "stream");
		stream = g_value_get_object(value);
		value = gst_structure_get_value(structure, "candidate");
		local_candidate = g_value_get_boxed(value);

		session = get_session_from_fs_stream(self, stream);

		purple_debug_info("backend-fs2",
				"got new local candidate: %s\n",
				local_candidate->foundation);

		g_object_get(stream, "participant", &participant, NULL);
		g_object_get(participant, "cname", &name, NULL);
		g_object_unref(participant);

		media_stream = get_stream(self, session->id, name);
		media_stream->local_candidates = g_list_append(
				media_stream->local_candidates,
				fs_candidate_copy(local_candidate));

		candidate = candidate_from_fs(local_candidate);
		g_signal_emit_by_name(self, "new-candidate",
				session->id, name, candidate);
		g_object_unref(candidate);
	} else if (gst_structure_has_name(structure,
			"farstream-local-candidates-prepared")) {
		const GValue *value;
		FsStream *stream;
		FsParticipant *participant;
		PurpleMediaBackendFs2Session *session;
		gchar *name;

		value = gst_structure_get_value(structure, "stream");
		stream = g_value_get_object(value);
		session = get_session_from_fs_stream(self, stream);

		g_object_get(stream, "participant", &participant, NULL);
		g_object_get(participant, "cname", &name, NULL);
		g_object_unref(participant);

		g_signal_emit_by_name(self, "candidates-prepared",
				session->id, name);
	} else if (gst_structure_has_name(structure,
			"farstream-new-active-candidate-pair")) {
		const GValue *value;
		FsStream *stream;
		FsCandidate *local_candidate;
		FsCandidate *remote_candidate;
		FsParticipant *participant;
		PurpleMediaBackendFs2Session *session;
		PurpleMediaCandidate *lcandidate, *rcandidate;
		gchar *name;

		value = gst_structure_get_value(structure, "stream");
		stream = g_value_get_object(value);
		value = gst_structure_get_value(structure, "local-candidate");
		local_candidate = g_value_get_boxed(value);
		value = gst_structure_get_value(structure, "remote-candidate");
		remote_candidate = g_value_get_boxed(value);

		g_object_get(stream, "participant", &participant, NULL);
		g_object_get(participant, "cname", &name, NULL);
		g_object_unref(participant);

		session = get_session_from_fs_stream(self, stream);

		lcandidate = candidate_from_fs(local_candidate);
		rcandidate = candidate_from_fs(remote_candidate);

		g_signal_emit_by_name(self, "active-candidate-pair",
				session->id, name, lcandidate, rcandidate);

		g_object_unref(lcandidate);
		g_object_unref(rcandidate);
	} else if (gst_structure_has_name(structure,
			"farstream-recv-codecs-changed")) {
		const GValue *value;
		GList *codecs;
		FsCodec *codec;

		value = gst_structure_get_value(structure, "codecs");
		codecs = g_value_get_boxed(value);
		codec = codecs->data;

		purple_debug_info("backend-fs2",
				"farstream-recv-codecs-changed: %s\n",
				codec->encoding_name);
	} else if (gst_structure_has_name(structure,
			"farstream-component-state-changed")) {
		const GValue *value;
		FsStreamState fsstate;
		guint component;
		const gchar *state;

		value = gst_structure_get_value(structure, "state");
		fsstate = g_value_get_enum(value);
		value = gst_structure_get_value(structure, "component");
		component = g_value_get_uint(value);

		switch (fsstate) {
			case FS_STREAM_STATE_FAILED:
				state = "FAILED";
				break;
			case FS_STREAM_STATE_DISCONNECTED:
				state = "DISCONNECTED";
				break;
			case FS_STREAM_STATE_GATHERING:
				state = "GATHERING";
				break;
			case FS_STREAM_STATE_CONNECTING:
				state = "CONNECTING";
				break;
			case FS_STREAM_STATE_CONNECTED:
				state = "CONNECTED";
				break;
			case FS_STREAM_STATE_READY:
				state = "READY";
				break;
			default:
				state = "UNKNOWN";
				break;
		}

		purple_debug_info("backend-fs2",
				"farstream-component-state-changed: "
				"component: %u state: %s\n",
				component, state);
	} else if (gst_structure_has_name(structure,
			"farstream-send-codec-changed")) {
		const GValue *value;
		FsCodec *codec;
		gchar *codec_str;

		value = gst_structure_get_value(structure, "codec");
		codec = g_value_get_boxed(value);
		codec_str = fs_codec_to_string(codec);

		purple_debug_info("backend-fs2",
				"farstream-send-codec-changed: codec: %s\n",
				codec_str);

		g_free(codec_str);
	} else if (gst_structure_has_name(structure,
			"farstream-codecs-changed")) {
		const GValue *value;
		FsSession *fssession;
		GList *sessions;

		value = gst_structure_get_value(structure, "session");
		fssession = g_value_get_object(value);
		sessions = g_hash_table_get_values(priv->sessions);

		for (; sessions; sessions =
				g_list_delete_link(sessions, sessions)) {
			PurpleMediaBackendFs2Session *session = sessions->data;
			gchar *session_id;

			if (session->session != fssession)
				continue;

			session_id = g_strdup(session->id);
			g_signal_emit_by_name(self, "codecs-changed",
					session_id);
			g_free(session_id);
			g_list_free(sessions);
			break;
		}
	}
}

static gboolean
downgrade_video_source(PurpleMediaBackendFs2 *self, GstBin *srcbin)
{
	PurpleMediaBackendFs2Private *priv;
	GstElement *src;
	GstPad *srcpad = NULL;
	GstPad *p;

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);

	src = gst_bin_get_by_name(srcbin, "purplevideodowngrade");
	if (src) {
		/* The failing source has already been downgraded. Stop here in
		 * order not to cause an infinite loop. */
		gst_object_unref(src);
		return FALSE;
	}

	src = gst_bin_get_by_name(srcbin, "tee");
	if (!src) {
		return FALSE;
	}

	while((p = gst_element_get_static_pad(src, "sink"))) {
		if (srcpad) {
			gst_object_unref(srcpad);
		}
		srcpad = gst_pad_get_peer(p);
		gst_object_unref(src);
		src = gst_pad_get_parent_element(srcpad);
		gst_object_unref(p);
	};

	if (srcpad) {
		PurpleMediaManager *manager;
		GstElement *pipeline;
		GstElement *dest;
		GstElement *newsrc;

		manager = purple_media_get_manager(priv->media);
		pipeline = purple_media_manager_get_pipeline(manager);

		dest = gst_pad_get_parent_element(GST_PAD_PEER(srcpad));

		remove_element(src);

		newsrc = gst_element_factory_make("videotestsrc",
				"purplevideodowngrade");
		g_object_set(newsrc, "is-live", TRUE, NULL);
		gst_bin_add_many(srcbin, newsrc, NULL);
		gst_element_link(newsrc, dest);
		gst_element_set_state(newsrc, GST_STATE_PLAYING);

		/* Pipeline with an error might not be any longer PLAYING. */
		gst_element_set_state(pipeline, GST_STATE_PLAYING);

		gst_object_unref(srcpad);

		/* Flush the video pipeline. */
		srcpad = gst_element_get_static_pad(newsrc, "src");
		gst_pad_push_event(srcpad, gst_event_new_flush_start());
		gst_pad_push_event(srcpad, gst_event_new_flush_stop(FALSE));
		gst_object_unref(srcpad);

		gst_object_unref(dest);
	}

	gst_object_unref(src);

	return TRUE;
}

static void
gst_handle_message_error(GstBus *bus, GstMessage *msg,
		PurpleMediaBackendFs2 *self)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	GstElement *element = GST_ELEMENT(GST_MESSAGE_SRC(msg));
	GstElement *lastElement = NULL;
	GList *sessions;
	gboolean fatal = TRUE;

	GError *error = NULL;
	gchar *debug_msg = NULL;

	gst_message_parse_error(msg, &error, &debug_msg);
	purple_debug_error("backend-fs2", "gst error %s\ndebugging: %s\n",
			error->message, debug_msg);

	g_error_free(error);
	g_free(debug_msg);

	while (element && !GST_IS_PIPELINE(element)) {
		if (element == priv->confbin)
			break;

		lastElement = element;
		element = GST_ELEMENT_PARENT(element);
	}

	if (!element || !GST_IS_PIPELINE(element))
		return;

	sessions = purple_media_get_session_ids(priv->media);

	for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
		PurpleMediaSessionType session_type;

		if (purple_media_get_src(priv->media, sessions->data)
				!= lastElement)
			continue;

		session_type = purple_media_get_session_type(priv->media,
				sessions->data);

		if (session_type & PURPLE_MEDIA_AUDIO)
			purple_media_error(priv->media,
					_("Error with your microphone"));
		else if (session_type & PURPLE_MEDIA_VIDEO) {
			fatal = !downgrade_video_source(self,
					GST_BIN(lastElement));

			if (fatal) {
				purple_media_error(priv->media,
						_("Error with your webcam"));
			}
		}

		break;
	}

	g_list_free(sessions);

	if (fatal) {
		purple_media_error(priv->media, _("Conference error"));
		purple_media_end(priv->media, NULL, NULL);
	}
}

static gboolean
gst_bus_cb(GstBus *bus, GstMessage *msg, PurpleMediaBackendFs2 *self)
{
	switch(GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_ELEMENT:
			gst_handle_message_element(bus, msg, self);
			break;
		case GST_MESSAGE_ERROR:
			gst_handle_message_error(bus, msg, self);
			break;
		default:
			break;
	}

	return TRUE;
}

static void
remove_element(GstElement *element)
{
	if (element) {
		gst_element_set_locked_state(element, TRUE);
		gst_element_set_state(element, GST_STATE_NULL);
		gst_bin_remove(GST_BIN(GST_ELEMENT_PARENT(element)), element);
	}
}

static void
state_changed_cb(PurpleMedia *media, PurpleMediaState state,
		gchar *sid, gchar *name, PurpleMediaBackendFs2 *self)
{
	if (state == PURPLE_MEDIA_STATE_END) {
		PurpleMediaBackendFs2Private *priv =
				PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);

		if (sid && name) {
			PurpleMediaBackendFs2Stream *stream = get_stream(self, sid, name);
			gst_object_unref(stream->stream);

			priv->streams = g_list_remove(priv->streams, stream);

			remove_element(stream->src);
			remove_element(stream->tee);
			remove_element(stream->volume);
			remove_element(stream->level);
			remove_element(stream->fakesink);
			remove_element(stream->queue);

			free_stream(stream);
		} else if (sid && !name) {
			PurpleMediaBackendFs2Session *session = get_session(self, sid);
			GstPad *pad;

			g_object_get(session->session, "sink-pad", &pad, NULL);
			gst_pad_unlink(GST_PAD_PEER(pad), pad);
			gst_object_unref(pad);

			gst_object_unref(session->session);
			g_hash_table_remove(priv->sessions, session->id);

			if (session->srcpad) {
				pad = gst_pad_get_peer(session->srcpad);
				if (pad) {
					gst_element_remove_pad(
							GST_PAD_PARENT(pad),
							pad);
					gst_object_unref(pad);
				}
				gst_object_unref(session->srcpad);
			}

			remove_element(session->srcvalve);
			remove_element(session->tee);

			free_session(session);
		}

		purple_media_manager_remove_output_windows(
				purple_media_get_manager(media), media, sid, name);
	}
}

static void
stream_info_cb(PurpleMedia *media, PurpleMediaInfoType type,
		gchar *sid, gchar *name, gboolean local,
		PurpleMediaBackendFs2 *self)
{
	if (type == PURPLE_MEDIA_INFO_ACCEPT && sid != NULL && name != NULL) {
		PurpleMediaBackendFs2Stream *stream =
				get_stream(self, sid, name);
		GError *err = NULL;

		g_object_set(G_OBJECT(stream->stream), "direction",
				session_type_to_fs_stream_direction(
				stream->session->type), NULL);

		if (stream->remote_candidates == NULL ||
				purple_media_is_initiator(media, sid, name))
			return;

		if (stream->supports_add)
			fs_stream_add_remote_candidates(stream->stream,
					stream->remote_candidates, &err);
		else
			fs_stream_force_remote_candidates(stream->stream,
					stream->remote_candidates, &err);

		if (err == NULL)
			return;

		purple_debug_error("backend-fs2", "Error adding "
				"remote candidates: %s\n",
				err->message);
		g_error_free(err);
	} else if (local == TRUE && (type == PURPLE_MEDIA_INFO_MUTE ||
			type == PURPLE_MEDIA_INFO_UNMUTE)) {
		PurpleMediaBackendFs2Private *priv =
				PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
		gboolean active = (type == PURPLE_MEDIA_INFO_MUTE);
		GList *sessions;

		if (sid == NULL)
			sessions = g_hash_table_get_values(priv->sessions);
		else
			sessions = g_list_prepend(NULL,
					get_session(self, sid));

		purple_debug_info("media", "Turning mute %s\n",
				active ? "on" : "off");

		for (; sessions; sessions = g_list_delete_link(
				sessions, sessions)) {
			PurpleMediaBackendFs2Session *session =
					sessions->data;

			if (session->type & PURPLE_MEDIA_SEND_AUDIO) {
				gchar *name = g_strdup_printf("volume_%s",
						session->id);
				GstElement *volume = gst_bin_get_by_name(
						GST_BIN(priv->confbin), name);
				g_free(name);
				g_object_set(volume, "mute", active, NULL);
			}
		}
	} else if (local == TRUE && (type == PURPLE_MEDIA_INFO_HOLD ||
			type == PURPLE_MEDIA_INFO_UNHOLD)) {
		gboolean active = (type == PURPLE_MEDIA_INFO_HOLD);
		GList *streams = get_streams(self, sid, name);
		for (; streams; streams =
				g_list_delete_link(streams, streams)) {
			PurpleMediaBackendFs2Stream *stream = streams->data;
			if (stream->session->type & PURPLE_MEDIA_SEND_AUDIO) {
				g_object_set(stream->stream, "direction",
						session_type_to_fs_stream_direction(
						stream->session->type & ((active) ?
						~PURPLE_MEDIA_SEND_AUDIO :
						PURPLE_MEDIA_AUDIO)), NULL);
			}
		}
	} else if (local == TRUE && (type == PURPLE_MEDIA_INFO_PAUSE ||
			type == PURPLE_MEDIA_INFO_UNPAUSE)) {
		gboolean active = (type == PURPLE_MEDIA_INFO_PAUSE);
		GList *streams = get_streams(self, sid, name);
		for (; streams; streams =
				g_list_delete_link(streams, streams)) {
			PurpleMediaBackendFs2Stream *stream = streams->data;
			if (stream->session->type & PURPLE_MEDIA_SEND_VIDEO) {
				g_object_set(stream->stream, "direction",
						session_type_to_fs_stream_direction(
						stream->session->type & ((active) ?
						~PURPLE_MEDIA_SEND_VIDEO :
						PURPLE_MEDIA_VIDEO)), NULL);
			}
		}
	}
}

static gboolean
init_conference(PurpleMediaBackendFs2 *self)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	GstElement *pipeline;
	GstBus *bus;
	gchar *name;
	GKeyFile *default_props;

	priv->conference = FS_CONFERENCE(
			gst_element_factory_make(priv->conference_type, NULL));

	if (priv->conference == NULL) {
		purple_debug_error("backend-fs2", "Conference == NULL\n");
		return FALSE;
	}

	if (purple_account_get_silence_suppression(
				purple_media_get_account(priv->media)))
		priv->silence_threshold = purple_prefs_get_int(
				"/purple/media/audio/silence_threshold") / 100.0;
	else
		priv->silence_threshold = 0;

	pipeline = purple_media_manager_get_pipeline(
			purple_media_get_manager(priv->media));

	if (pipeline == NULL) {
		purple_debug_error("backend-fs2",
				"Couldn't retrieve pipeline.\n");
		return FALSE;
	}

	name = g_strdup_printf("conf_%p", priv->conference);
	priv->confbin = gst_bin_new(name);
	if (priv->confbin == NULL) {
		purple_debug_error("backend-fs2",
				"Couldn't create confbin.\n");
		return FALSE;
	}

	g_free(name);

	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	if (bus == NULL) {
		purple_debug_error("backend-fs2",
				"Couldn't get the pipeline's bus.\n");
		return FALSE;
	}

	default_props = fs_utils_get_default_element_properties(GST_ELEMENT(priv->conference));
	if (default_props != NULL) {
		priv->notifier = fs_element_added_notifier_new();
		fs_element_added_notifier_add(priv->notifier,
				GST_BIN(priv->confbin));
		fs_element_added_notifier_set_properties_from_keyfile(priv->notifier, default_props);
	}

	g_signal_connect(G_OBJECT(bus), "message",
			G_CALLBACK(gst_bus_cb), self);
	gst_object_unref(bus);

	if (!gst_bin_add(GST_BIN(pipeline),
			GST_ELEMENT(priv->confbin))) {
		purple_debug_error("backend-fs2", "Couldn't add confbin "
				"element to the pipeline\n");
		return FALSE;
	}

	if (!gst_bin_add(GST_BIN(priv->confbin),
			GST_ELEMENT(priv->conference))) {
		purple_debug_error("backend-fs2", "Couldn't add conference "
				"element to the confbin\n");
		return FALSE;
	}

	if (gst_element_set_state(GST_ELEMENT(priv->confbin),
			GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		purple_debug_error("backend-fs2",
				"Failed to start conference.\n");
		return FALSE;
	}

	return TRUE;
}

static gboolean
create_src(PurpleMediaBackendFs2 *self, const gchar *sess_id,
		PurpleMediaSessionType type)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	PurpleMediaBackendFs2Session *session;
	PurpleMediaSessionType session_type;
	FsMediaType media_type = session_type_to_fs_media_type(type);
	FsStreamDirection type_direction =
			session_type_to_fs_stream_direction(type);
	GstElement *src;
	GstPad *sinkpad, *srcpad;
	GstPad *ghost = NULL;

	if ((type_direction & FS_DIRECTION_SEND) == 0)
		return TRUE;

	session_type = session_type_from_fs(
			media_type, FS_DIRECTION_SEND);
	src = purple_media_manager_get_element(
			purple_media_get_manager(priv->media),
			session_type, priv->media, sess_id, NULL);

	if (!GST_IS_ELEMENT(src)) {
		purple_debug_error("backend-fs2",
				"Error creating src for session %s\n",
				sess_id);
		return FALSE;
	}

	session = get_session(self, sess_id);

	if (session == NULL) {
		purple_debug_warning("backend-fs2",
				"purple_media_set_src: trying to set"
				" src on non-existent session\n");
		return FALSE;
	}

	if (session->src)
		gst_object_unref(session->src);

	session->src = src;
	gst_element_set_locked_state(session->src, TRUE);

	session->tee = gst_element_factory_make("tee", NULL);
	gst_bin_add(GST_BIN(priv->confbin), session->tee);

	/* This supposedly isn't necessary, but it silences some warnings */
	if (GST_ELEMENT_PARENT(priv->confbin)
			== GST_ELEMENT_PARENT(session->src)) {
		GstPad *pad = gst_element_get_static_pad(session->tee, "sink");
		ghost = gst_ghost_pad_new(NULL, pad);
		gst_object_unref(pad);
		gst_pad_set_active(ghost, TRUE);
		gst_element_add_pad(priv->confbin, ghost);
	}

	gst_element_set_state(session->tee, GST_STATE_PLAYING);
	gst_element_link(session->src, priv->confbin);
	if (ghost)
		session->srcpad = gst_pad_get_peer(ghost);

	g_object_get(session->session, "sink-pad", &sinkpad, NULL);
	if (session->type & PURPLE_MEDIA_SEND_AUDIO) {
		gchar *name = g_strdup_printf("volume_%s", session->id);
		GstElement *level;
		GstElement *volume = gst_element_factory_make("volume", name);
		double input_volume = purple_prefs_get_int(
				"/purple/media/audio/volume/input")/10.0;
		g_free(name);
		name = g_strdup_printf("sendlevel_%s", session->id);
		level = gst_element_factory_make("level", name);
		g_free(name);
		session->srcvalve = gst_element_factory_make("valve", NULL);
		gst_bin_add(GST_BIN(priv->confbin), volume);
		gst_bin_add(GST_BIN(priv->confbin), level);
		gst_bin_add(GST_BIN(priv->confbin), session->srcvalve);
		gst_element_set_state(level, GST_STATE_PLAYING);
		gst_element_set_state(volume, GST_STATE_PLAYING);
		gst_element_set_state(session->srcvalve, GST_STATE_PLAYING);
		gst_element_link(level, session->srcvalve);
		gst_element_link(volume, level);
		gst_element_link(session->tee, volume);
		srcpad = gst_element_get_static_pad(session->srcvalve, "src");
		g_object_set(volume, "volume", input_volume, NULL);
	} else {
		srcpad = gst_element_get_request_pad(session->tee, "src_%u");
	}

	purple_debug_info("backend-fs2", "connecting pad: %s\n",
			  gst_pad_link(srcpad, sinkpad) == GST_PAD_LINK_OK
			  ? "success" : "failure");
	gst_element_set_locked_state(session->src, FALSE);
	gst_object_unref(session->src);
	gst_object_unref(sinkpad);

	purple_media_manager_create_output_window(purple_media_get_manager(
			priv->media), priv->media, sess_id, NULL);

	purple_debug_info("backend-fs2", "create_src: setting source "
		"state to GST_STATE_PLAYING - it may hang here on win32\n");
	gst_element_set_state(session->src, GST_STATE_PLAYING);
	purple_debug_info("backend-fs2", "create_src: state set\n");

	return TRUE;
}

static gboolean
create_session(PurpleMediaBackendFs2 *self, const gchar *sess_id,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	PurpleMediaBackendFs2Session *session;
	GError *err = NULL;
	GList *codec_conf = NULL;
	gchar *filename = NULL;

	session = g_new0(PurpleMediaBackendFs2Session, 1);

	session->session = fs_conference_new_session(priv->conference,
			session_type_to_fs_media_type(type), &err);

#ifdef HAVE_MEDIA_APPLICATION
	if (type == PURPLE_MEDIA_APPLICATION) {
		GstCaps *caps;
		GObject *rtpsession = NULL;

		caps = gst_caps_new_empty_simple ("application/octet-stream");
		fs_session_set_allowed_caps (session->session, caps, caps, NULL);
		gst_caps_unref (caps);
		g_object_get (session->session, "internal-session", &rtpsession, NULL);
		if (rtpsession) {
			g_object_set (rtpsession, "probation", 0, NULL);
			g_object_unref (rtpsession);
		}
	}
#endif
	if (err != NULL) {
		purple_media_error(priv->media,
				_("Error creating session: %s"),
				err->message);
		g_error_free(err);
		g_free(session);
		return FALSE;
	}

	filename = g_build_filename(purple_user_dir(), "fs-codec.conf", NULL);
	codec_conf = fs_codec_list_from_keyfile(filename, &err);
	g_free(filename);

	if (err != NULL) {
		if (err->code == G_KEY_FILE_ERROR_NOT_FOUND)
			purple_debug_info("backend-fs2", "Couldn't read "
					"fs-codec.conf: %s\n",
					err->message);
		else
			purple_debug_error("backend-fs2", "Error reading "
					"fs-codec.conf: %s\n",
					err->message);
		g_error_free(err);

		purple_debug_info("backend-fs2",
				"Loading default codec conf instead\n");
		codec_conf = fs_utils_get_default_codec_preferences(
				GST_ELEMENT(priv->conference));
	}

	fs_session_set_codec_preferences(session->session, codec_conf, NULL);
	fs_codec_list_destroy(codec_conf);

	/*
	 * Removes a 5-7 second delay before
	 * receiving the src-pad-added signal.
	 * Only works for non-multicast FsRtpSessions.
	 */
	if (!!strcmp(transmitter, "multicast"))
		g_object_set(G_OBJECT(session->session),
				"no-rtcp-timeout", 0, NULL);

	session->id = g_strdup(sess_id);
	session->backend = self;
	session->type = type;

	if (!priv->sessions) {
		purple_debug_info("backend-fs2",
				"Creating hash table for sessions\n");
		priv->sessions = g_hash_table_new_full(g_str_hash, g_str_equal,
		                                       g_free, NULL);
	}

	g_hash_table_insert(priv->sessions, g_strdup(session->id), session);

	if (!create_src(self, sess_id, type)) {
		purple_debug_info("backend-fs2", "Error creating the src\n");
		return FALSE;
	}

	return TRUE;
}

static void
free_session(PurpleMediaBackendFs2Session *session)
{
	g_free(session->id);
	g_free(session);
}

static gboolean
create_participant(PurpleMediaBackendFs2 *self, const gchar *name)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	FsParticipant *participant;
	GError *err = NULL;

	participant = fs_conference_new_participant(
			priv->conference, &err);

	if (err) {
		purple_debug_error("backend-fs2",
				"Error creating participant: %s\n",
				err->message);
		g_error_free(err);
		return FALSE;
	}

	if (g_object_class_find_property(G_OBJECT_GET_CLASS(participant),
			"cname")) {
		g_object_set(participant, "cname", name, NULL);
	}

	if (!priv->participants) {
		purple_debug_info("backend-fs2",
				"Creating hash table for participants\n");
		priv->participants = g_hash_table_new_full(g_str_hash,
				g_str_equal, g_free, g_object_unref);
	}

	g_hash_table_insert(priv->participants, g_strdup(name), participant);

	return TRUE;
}

static gboolean
src_pad_added_cb_cb(PurpleMediaBackendFs2Stream *stream)
{
	PurpleMediaBackendFs2Private *priv;

	g_return_val_if_fail(stream != NULL, FALSE);

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(stream->session->backend);
	stream->connected_cb_id = 0;

	purple_media_manager_create_output_window(
			purple_media_get_manager(priv->media), priv->media,
			stream->session->id, stream->participant);

	g_signal_emit_by_name(priv->media, "state-changed",
			PURPLE_MEDIA_STATE_CONNECTED,
			stream->session->id, stream->participant);
	return FALSE;
}

static void
src_pad_added_cb(FsStream *fsstream, GstPad *srcpad,
		FsCodec *codec, PurpleMediaBackendFs2Stream *stream)
{
	PurpleMediaBackendFs2Private *priv;
	GstPad *sinkpad;

	g_return_if_fail(FS_IS_STREAM(fsstream));
	g_return_if_fail(stream != NULL);

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(stream->session->backend);

	if (stream->src == NULL) {
		GstElement *sink = NULL;

		if (codec->media_type == FS_MEDIA_TYPE_AUDIO) {
			double output_volume = purple_prefs_get_int(
					"/purple/media/audio/volume/output")/10.0;
			/*
			 * Should this instead be:
			 *  audioconvert ! audioresample ! liveadder !
			 *   audioresample ! audioconvert ! realsink
			 */
			stream->queue = gst_element_factory_make("queue", NULL);
			stream->volume = gst_element_factory_make("volume", NULL);
			g_object_set(stream->volume, "volume", output_volume, NULL);
			stream->level = gst_element_factory_make("level", NULL);
			stream->src = gst_element_factory_make("liveadder", NULL);
			sink = purple_media_manager_get_element(
					purple_media_get_manager(priv->media),
					PURPLE_MEDIA_RECV_AUDIO, priv->media,
					stream->session->id,
					stream->participant);
			gst_bin_add(GST_BIN(priv->confbin), stream->queue);
			gst_bin_add(GST_BIN(priv->confbin), stream->volume);
			gst_bin_add(GST_BIN(priv->confbin), stream->level);
			gst_bin_add(GST_BIN(priv->confbin), sink);
			gst_element_set_state(sink, GST_STATE_PLAYING);
			gst_element_set_state(stream->level, GST_STATE_PLAYING);
			gst_element_set_state(stream->volume, GST_STATE_PLAYING);
			gst_element_set_state(stream->queue, GST_STATE_PLAYING);
			gst_element_link(stream->level, sink);
			gst_element_link(stream->volume, stream->level);
			gst_element_link(stream->queue, stream->volume);
			sink = stream->queue;
		} else if (codec->media_type == FS_MEDIA_TYPE_VIDEO) {
			stream->src = gst_element_factory_make("funnel", NULL);
			sink = gst_element_factory_make("fakesink", NULL);
			g_object_set(G_OBJECT(sink), "async", FALSE, NULL);
			gst_bin_add(GST_BIN(priv->confbin), sink);
			gst_element_set_state(sink, GST_STATE_PLAYING);
			stream->fakesink = sink;
#ifdef HAVE_MEDIA_APPLICATION
		} else if (codec->media_type == FS_MEDIA_TYPE_APPLICATION) {
			stream->src = gst_element_factory_make("funnel", NULL);
			sink = purple_media_manager_get_element(
					purple_media_get_manager(priv->media),
					PURPLE_MEDIA_RECV_APPLICATION, priv->media,
					stream->session->id,
					stream->participant);
			gst_bin_add(GST_BIN(priv->confbin), sink);
			gst_element_set_state(sink, GST_STATE_PLAYING);
#endif
		}
		stream->tee = gst_element_factory_make("tee", NULL);
		gst_bin_add_many(GST_BIN(priv->confbin),
				stream->src, stream->tee, NULL);
		gst_element_set_state(stream->tee, GST_STATE_PLAYING);
		gst_element_set_state(stream->src, GST_STATE_PLAYING);
		gst_element_link_many(stream->src, stream->tee, sink, NULL);
	}

	sinkpad = gst_element_get_request_pad(stream->src, "sink_%u");
	gst_pad_link(srcpad, sinkpad);
	gst_object_unref(sinkpad);

	stream->connected_cb_id = purple_timeout_add(0,
			(GSourceFunc)src_pad_added_cb_cb, stream);
}

static GValueArray *
append_relay_info(GValueArray *relay_info, const gchar *ip, gint port,
	const gchar *username, const gchar *password, const gchar *type)
{
	GValue value;
	GstStructure *turn_setup = gst_structure_new("relay-info",
				"ip", G_TYPE_STRING, ip,
				"port", G_TYPE_UINT, port,
				"username", G_TYPE_STRING, username,
				"password", G_TYPE_STRING, password,
				"relay-type", G_TYPE_STRING, type,
				NULL);

	if (turn_setup) {
		memset(&value, 0, sizeof(GValue));
		g_value_init(&value, GST_TYPE_STRUCTURE);
		gst_value_set_structure(&value, turn_setup);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		relay_info = g_value_array_append(relay_info, &value);
G_GNUC_END_IGNORE_DEPRECATIONS
		gst_structure_free(turn_setup);
	}

	return relay_info;
}

static gboolean
create_stream(PurpleMediaBackendFs2 *self,
		const gchar *sess_id, const gchar *who,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter,
		guint num_params, GParameter *params)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	GError *err = NULL;
	FsStream *fsstream = NULL;
	const gchar *stun_ip = purple_network_get_stun_ip();
	const gchar *turn_ip = purple_network_get_turn_ip();
	guint _num_params = num_params;
	GParameter *_params;
	FsStreamDirection type_direction =
			session_type_to_fs_stream_direction(type);
	PurpleMediaBackendFs2Session *session;
	PurpleMediaBackendFs2Stream *stream;
	FsParticipant *participant;
	/* check if the protocol has already specified a relay-info
	  we need to do this to allow them to override when using non-standard
	  TURN modes, like Google f.ex. */
	gboolean got_turn_from_protocol = FALSE;
	guint i;

	session = get_session(self, sess_id);

	if (session == NULL) {
		purple_debug_error("backend-fs2",
				"Couldn't find session to create stream.\n");
		return FALSE;
	}

	participant = get_participant(self, who);

	if (participant == NULL) {
		purple_debug_error("backend-fs2", "Couldn't find "
				"participant to create stream.\n");
		return FALSE;
	}

	fsstream = fs_session_new_stream(session->session, participant,
			initiator == TRUE ? type_direction :
			(type_direction & FS_DIRECTION_RECV), &err);

	if (fsstream == NULL) {
		if (err) {
			purple_debug_error("backend-fs2",
					"Error creating stream: %s\n",
					err && err->message ?
					err->message : "NULL");
			g_error_free(err);
		} else
			purple_debug_error("backend-fs2",
					"Error creating stream\n");
		return FALSE;
	}

	for (i = 0 ; i < num_params ; i++) {
		if (purple_strequal(params[i].name, "relay-info")) {
			got_turn_from_protocol = TRUE;
			break;
		}
	}

	_params = g_new0(GParameter, num_params + 3);
	memcpy(_params, params, sizeof(GParameter) * num_params);

	/* set the controlling mode parameter */
	_params[_num_params].name = "controlling-mode";
	g_value_init(&_params[_num_params].value, G_TYPE_BOOLEAN);
	g_value_set_boolean(&_params[_num_params].value, initiator);
	++_num_params;

	if (stun_ip) {
		purple_debug_info("backend-fs2",
			"Setting stun-ip on new stream: %s\n", stun_ip);

		_params[_num_params].name = "stun-ip";
		g_value_init(&_params[_num_params].value, G_TYPE_STRING);
		g_value_set_string(&_params[_num_params].value, stun_ip);
		++_num_params;
	}

	if (turn_ip && !strcmp("nice", transmitter) && !got_turn_from_protocol) {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		GValueArray *relay_info = g_value_array_new(0);
G_GNUC_END_IGNORE_DEPRECATIONS
		gint port;
		const gchar *username =	purple_prefs_get_string(
				"/purple/network/turn_username");
		const gchar *password = purple_prefs_get_string(
				"/purple/network/turn_password");

		/* UDP */
		port = purple_prefs_get_int("/purple/network/turn_port");
		if (port > 0) {
			relay_info = append_relay_info(relay_info, turn_ip, port, username,
				password, "udp");
		}
		
		/* TCP */
		port = purple_prefs_get_int("/purple/network/turn_port_tcp");
		if (port > 0) {
			relay_info = append_relay_info(relay_info, turn_ip, port, username,
				password, "tcp");
		}

		/* TURN over SSL is only supported by libnice for Google's "psuedo" SSL mode
			at this time */

		purple_debug_info("backend-fs2",
			"Setting relay-info on new stream\n");
		_params[_num_params].name = "relay-info";
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		g_value_init(&_params[_num_params].value, G_TYPE_VALUE_ARRAY);
		g_value_set_boxed(&_params[_num_params].value, relay_info);
		g_value_array_free(relay_info);
G_GNUC_END_IGNORE_DEPRECATIONS
		_num_params++;
	}

	if (!fs_stream_set_transmitter(fsstream, transmitter,
			_params, _num_params, &err)) {
		purple_debug_error("backend-fs2",
			"Could not set transmitter %s: %s.\n",
			transmitter, err ? err->message : NULL);
		g_clear_error(&err);
		g_free(_params);
		return FALSE;
	}
	g_free(_params);

	stream = g_new0(PurpleMediaBackendFs2Stream, 1);
	stream->participant = g_strdup(who);
	stream->session = session;
	stream->stream = fsstream;
	stream->supports_add = !strcmp(transmitter, "nice");

	priv->streams =	g_list_append(priv->streams, stream);

	g_signal_connect(G_OBJECT(fsstream), "src-pad-added",
			G_CALLBACK(src_pad_added_cb), stream);

	return TRUE;
}

static void
free_stream(PurpleMediaBackendFs2Stream *stream)
{
	/* Remove the connected_cb timeout */
	if (stream->connected_cb_id != 0)
		purple_timeout_remove(stream->connected_cb_id);

	g_free(stream->participant);

	if (stream->local_candidates)
		fs_candidate_list_destroy(stream->local_candidates);

	if (stream->remote_candidates)
		fs_candidate_list_destroy(stream->remote_candidates);

	g_free(stream);
}

static gboolean
purple_media_backend_fs2_add_stream(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *who,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter,
		guint num_params, GParameter *params)
{
	PurpleMediaBackendFs2 *backend = PURPLE_MEDIA_BACKEND_FS2(self);
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(backend);
	PurpleMediaBackendFs2Stream *stream;

	if (priv->conference == NULL && !init_conference(backend)) {
		purple_debug_error("backend-fs2",
				"Error initializing the conference.\n");
		return FALSE;
	}

	if (get_session(backend, sess_id) == NULL &&
			!create_session(backend, sess_id, type,
			initiator, transmitter)) {
		purple_debug_error("backend-fs2",
				"Error creating the session.\n");
		return FALSE;
	}

	if (get_participant(backend, who) == NULL &&
			!create_participant(backend, who)) {
		purple_debug_error("backend-fs2",
				"Error creating the participant.\n");
		return FALSE;
	}

	stream = get_stream(backend, sess_id, who);

	if (stream != NULL) {
		FsStreamDirection type_direction =
				session_type_to_fs_stream_direction(type);

		if (session_type_to_fs_stream_direction(
				stream->session->type) != type_direction) {
			/* change direction */
			g_object_set(stream->stream, "direction",
					type_direction, NULL);
		}
	} else if (!create_stream(backend, sess_id, who, type,
			initiator, transmitter, num_params, params)) {
		purple_debug_error("backend-fs2",
				"Error creating the stream.\n");
		return FALSE;
	}

	return TRUE;
}

static void
purple_media_backend_fs2_add_remote_candidates(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *remote_candidates)
{
	PurpleMediaBackendFs2Private *priv;
	PurpleMediaBackendFs2Stream *stream;
	GError *err = NULL;

	g_return_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self));

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	stream = get_stream(PURPLE_MEDIA_BACKEND_FS2(self),
			sess_id, participant);

	if (stream == NULL) {
		purple_debug_error("backend-fs2",
				"purple_media_add_remote_candidates: "
				"couldn't find stream %s %s.\n",
				sess_id ? sess_id : "(null)",
				participant ? participant : "(null)");
		return;
	}

	stream->remote_candidates = g_list_concat(stream->remote_candidates,
			candidate_list_to_fs(remote_candidates));

	if (purple_media_is_initiator(priv->media, sess_id, participant) ||
			purple_media_accepted(
			priv->media, sess_id, participant)) {

		if (stream->supports_add)
			fs_stream_add_remote_candidates(stream->stream,
					stream->remote_candidates, &err);
		else
			fs_stream_force_remote_candidates(stream->stream,
					stream->remote_candidates, &err);

		if (err) {
			purple_debug_error("backend-fs2", "Error adding remote"
					" candidates: %s\n", err->message);
			g_error_free(err);
		}
	}
}

static gboolean
purple_media_backend_fs2_codecs_ready(PurpleMediaBackend *self,
		const gchar *sess_id)
{
	PurpleMediaBackendFs2Private *priv;
	gboolean ret = FALSE;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), FALSE);

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);

	if (sess_id != NULL) {
		PurpleMediaBackendFs2Session *session = get_session(
				PURPLE_MEDIA_BACKEND_FS2(self), sess_id);

		if (session == NULL)
			return FALSE;

		if (session->type & (PURPLE_MEDIA_SEND_AUDIO |
#ifdef HAVE_MEDIA_APPLICATION
				PURPLE_MEDIA_SEND_APPLICATION |
#endif
				PURPLE_MEDIA_SEND_VIDEO)) {

			GList *codecs = NULL;

			g_object_get(session->session,
					"codecs", &codecs, NULL);
			if (codecs) {
				fs_codec_list_destroy (codecs);
				ret = TRUE;
			}
		} else
			ret = TRUE;
	} else {
		GList *values = g_hash_table_get_values(priv->sessions);

		for (; values; values = g_list_delete_link(values, values)) {
			PurpleMediaBackendFs2Session *session = values->data;

			if (session->type & (PURPLE_MEDIA_SEND_AUDIO |
#ifdef HAVE_MEDIA_APPLICATION
					PURPLE_MEDIA_SEND_APPLICATION |
#endif
					PURPLE_MEDIA_SEND_VIDEO)) {

				GList *codecs = NULL;

				g_object_get(session->session,
						"codecs", &codecs, NULL);
				if (codecs) {
					fs_codec_list_destroy (codecs);
					ret = TRUE;
				} else {
					ret = FALSE;
					break;
				}
			} else
				ret = TRUE;
		}

		if (values != NULL)
			g_list_free(values);
	}

	return ret;
}

static GList *
purple_media_backend_fs2_get_codecs(PurpleMediaBackend *self,
		const gchar *sess_id)
{
	PurpleMediaBackendFs2Session *session;
	GList *fscodecs;
	GList *codecs;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), NULL);

	session = get_session(PURPLE_MEDIA_BACKEND_FS2(self), sess_id);

	if (session == NULL)
		return NULL;

	g_object_get(G_OBJECT(session->session),
		     "codecs", &fscodecs, NULL);
	codecs = codec_list_from_fs(fscodecs);
	fs_codec_list_destroy(fscodecs);

	return codecs;
}

static GList *
purple_media_backend_fs2_get_local_candidates(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant)
{
	PurpleMediaBackendFs2Stream *stream;
	GList *candidates = NULL;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), NULL);

	stream = get_stream(PURPLE_MEDIA_BACKEND_FS2(self),
			sess_id, participant);

	if (stream != NULL)
		candidates = candidate_list_from_fs(
				stream->local_candidates);
	return candidates;
}

static gboolean
purple_media_backend_fs2_set_remote_codecs(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *codecs)
{
	PurpleMediaBackendFs2Stream *stream;
	GList *fscodecs;
	GError *err = NULL;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), FALSE);
	stream = get_stream(PURPLE_MEDIA_BACKEND_FS2(self),
			sess_id, participant);

	if (stream == NULL)
		return FALSE;

	fscodecs = codec_list_to_fs(codecs);
	fs_stream_set_remote_codecs(stream->stream, fscodecs, &err);
	fs_codec_list_destroy(fscodecs);

	if (err) {
		purple_debug_error("backend-fs2",
				"Error setting remote codecs: %s\n",
				err->message);
		g_error_free(err);
		return FALSE;
	}

	return TRUE;
}

static GstStructure *
create_fs2_srtp_structure(const gchar *cipher, const gchar *auth,
	const gchar *key, gsize key_len)
{
	GstStructure *result;
	GstBuffer *buffer;
	GstMapInfo info;

	buffer = gst_buffer_new_allocate(NULL, key_len, NULL);
	gst_buffer_map(buffer, &info, GST_MAP_WRITE);
	memcpy(info.data, key, key_len);
	gst_buffer_unmap(buffer, &info);

	result = gst_structure_new("FarstreamSRTP",
			"cipher", G_TYPE_STRING, cipher,
			"auth", G_TYPE_STRING, auth,
			"key", GST_TYPE_BUFFER, buffer,
			NULL);
	gst_buffer_unref(buffer);

	return result;
}

static gboolean
purple_media_backend_fs2_set_encryption_parameters (PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *cipher, const gchar *auth,
		const gchar *key, gsize key_len)
{
	PurpleMediaBackendFs2Session *session;
	GstStructure *srtp;
	GError *err = NULL;
	gboolean result;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), FALSE);

	session = get_session(PURPLE_MEDIA_BACKEND_FS2(self), sess_id);
	if (!session)
		return FALSE;

	srtp = create_fs2_srtp_structure(cipher, auth, key, key_len);
	if (!srtp)
		return FALSE;

	result = fs_session_set_encryption_parameters(session->session, srtp,
						&err);
	if (!result) {
		purple_debug_error("backend-fs2",
				"Error setting encryption parameters: %s\n", err->message);
		g_error_free(err);
	}

	gst_structure_free(srtp);
	return result;
}

static gboolean
purple_media_backend_fs2_set_decryption_parameters (PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		const gchar *cipher, const gchar *auth,
		const gchar *key, gsize key_len)
{
	PurpleMediaBackendFs2Stream *stream;
	GstStructure *srtp;
	GError *err = NULL;
	gboolean result;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), FALSE);

	stream = get_stream(PURPLE_MEDIA_BACKEND_FS2(self), sess_id,
			participant);
	if (!stream)
		return FALSE;

	srtp = create_fs2_srtp_structure(cipher, auth, key, key_len);
	if (!srtp)
		return FALSE;

	result = fs_stream_set_decryption_parameters(stream->stream, srtp,
						&err);
	if (!result) {
		purple_debug_error("backend-fs2",
				"Error setting decryption parameters: %s\n", err->message);
		g_error_free(err);
	}

	gst_structure_free(srtp);
	return result;
}

static gboolean
purple_media_backend_fs2_set_send_codec(PurpleMediaBackend *self,
		const gchar *sess_id, PurpleMediaCodec *codec)
{
	PurpleMediaBackendFs2Session *session;
	FsCodec *fscodec;
	GError *err = NULL;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), FALSE);

	session = get_session(PURPLE_MEDIA_BACKEND_FS2(self), sess_id);

	if (session == NULL)
		return FALSE;

	fscodec = codec_to_fs(codec);
	fs_session_set_send_codec(session->session, fscodec, &err);
	fs_codec_destroy(fscodec);

	if (err) {
		purple_debug_error("media", "Error setting send codec\n");
		g_error_free(err);
		return FALSE;
	}

	return TRUE;
}

static const gchar **
purple_media_backend_fs2_get_available_params(void)
{
	static const gchar *supported_params[] = {
		"sdes-cname", "sdes-email", "sdes-location", "sdes-name", "sdes-note",
		"sdes-phone", "sdes-tool", NULL
	};

	return supported_params;
}

static const gchar*
param_to_sdes_type(const gchar *param)
{
	const gchar **supported = purple_media_backend_fs2_get_available_params();
	static const gchar *sdes_types[] = {
		"cname", "email", "location", "name", "note", "phone", "tool", NULL
	};
	guint i;

	for (i = 0; supported[i] != NULL; ++i) {
		if (!strcmp(param, supported[i])) {
			return sdes_types[i];
		}
	}

	return NULL;
}

static void
purple_media_backend_fs2_set_params(PurpleMediaBackend *self,
		guint num_params, GParameter *params)
{
	PurpleMediaBackendFs2Private *priv;
	guint i;
	GstStructure *sdes;

	g_return_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self));

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);

	if (priv->conference == NULL &&
		!init_conference(PURPLE_MEDIA_BACKEND_FS2(self))) {
		purple_debug_error("backend-fs2",
				"Error initializing the conference.\n");
		return;
	}

	g_object_get(G_OBJECT(priv->conference), "sdes", &sdes, NULL);

	for (i = 0; i != num_params; ++i) {
		const gchar *sdes_type = param_to_sdes_type(params[i].name);
		if (!sdes_type)
			continue;

		gst_structure_set(sdes, sdes_type,
		                  G_TYPE_STRING, g_value_get_string(&params[i].value),
		                  NULL);
	}

	g_object_set(G_OBJECT(priv->conference), "sdes", sdes, NULL);
	gst_structure_free(sdes);
}
static gboolean
send_dtmf_callback(gpointer userdata)
{
	FsSession *session = userdata;

	fs_session_stop_telephony_event(session);

	return FALSE;
}
static gboolean
purple_media_backend_fs2_send_dtmf(PurpleMediaBackend *self,
		const gchar *sess_id, gchar dtmf, guint8 volume,
		guint16 duration)
{
	PurpleMediaBackendFs2Session *session;
	FsDTMFEvent event;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), FALSE);

	session = get_session(PURPLE_MEDIA_BACKEND_FS2(self), sess_id);
	if (session == NULL)
		return FALSE;

	/* Convert DTMF char into FsDTMFEvent enum */
	switch(dtmf) {
		case '0': event = FS_DTMF_EVENT_0; break;
		case '1': event = FS_DTMF_EVENT_1; break;
		case '2': event = FS_DTMF_EVENT_2; break;
		case '3': event = FS_DTMF_EVENT_3; break;
		case '4': event = FS_DTMF_EVENT_4; break;
		case '5': event = FS_DTMF_EVENT_5; break;
		case '6': event = FS_DTMF_EVENT_6; break;
		case '7': event = FS_DTMF_EVENT_7; break;
		case '8': event = FS_DTMF_EVENT_8; break;
		case '9': event = FS_DTMF_EVENT_9; break;
		case '*': event = FS_DTMF_EVENT_STAR; break;
		case '#': event = FS_DTMF_EVENT_POUND; break;
		case 'A': event = FS_DTMF_EVENT_A; break;
		case 'B': event = FS_DTMF_EVENT_B; break;
		case 'C': event = FS_DTMF_EVENT_C; break;
		case 'D': event = FS_DTMF_EVENT_D; break;
		default:
			return FALSE;
	}

	if (!fs_session_start_telephony_event(session->session,
			event, volume)) {
		return FALSE;
	}

	if (duration <= 50) {
		fs_session_stop_telephony_event(session->session);
	} else {
		purple_timeout_add(duration, send_dtmf_callback,
				session->session);
	}

	return TRUE;
}
#else
GType
purple_media_backend_fs2_get_type(void)
{
	return G_TYPE_NONE;
}
#endif /* USE_VV */

GstElement *
purple_media_backend_fs2_get_src(PurpleMediaBackendFs2 *self,
		const gchar *sess_id)
{
#ifdef USE_VV
	PurpleMediaBackendFs2Session *session = get_session(self, sess_id);
	return session != NULL ? session->src : NULL;
#else
	return NULL;
#endif
}

GstElement *
purple_media_backend_fs2_get_tee(PurpleMediaBackendFs2 *self,
		const gchar *sess_id, const gchar *who)
{
#ifdef USE_VV
	if (sess_id != NULL && who == NULL) {
		PurpleMediaBackendFs2Session *session =
				get_session(self, sess_id);
		return (session != NULL) ? session->tee : NULL;
	} else if (sess_id != NULL && who != NULL) {
		PurpleMediaBackendFs2Stream *stream =
				get_stream(self, sess_id, who);
		return (stream != NULL) ? stream->tee : NULL;
	}

#endif /* USE_VV */
	g_return_val_if_reached(NULL);
}

void
purple_media_backend_fs2_set_input_volume(PurpleMediaBackendFs2 *self,
		const gchar *sess_id, double level)
{
#ifdef USE_VV
	PurpleMediaBackendFs2Private *priv;
	GList *sessions;

	g_return_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self));

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);

	purple_prefs_set_int("/purple/media/audio/volume/input", level);

	if (sess_id == NULL)
		sessions = g_hash_table_get_values(priv->sessions);
	else
		sessions = g_list_append(NULL, get_session(self, sess_id));

	for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
		PurpleMediaBackendFs2Session *session = sessions->data;

		if (session->type & PURPLE_MEDIA_SEND_AUDIO) {
			gchar *name = g_strdup_printf("volume_%s",
					session->id);
			GstElement *volume = gst_bin_get_by_name(
					GST_BIN(priv->confbin), name);
			g_free(name);
			g_object_set(volume, "volume", level/10.0, NULL);
		}
	}
#endif /* USE_VV */
}

void
purple_media_backend_fs2_set_output_volume(PurpleMediaBackendFs2 *self,
		const gchar *sess_id, const gchar *who, double level)
{
#ifdef USE_VV
	GList *streams;

	g_return_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self));

	purple_prefs_set_int("/purple/media/audio/volume/output", level);

	streams = get_streams(self, sess_id, who);

	for (; streams; streams = g_list_delete_link(streams, streams)) {
		PurpleMediaBackendFs2Stream *stream = streams->data;

		if (stream->session->type & PURPLE_MEDIA_RECV_AUDIO
				&& GST_IS_ELEMENT(stream->volume)) {
			g_object_set(stream->volume, "volume",
					level/10.0, NULL);
		}
	}
#endif /* USE_VV */
}

#ifdef USE_VV
static gboolean
purple_media_backend_fs2_set_send_rtcp_mux(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		gboolean send_rtcp_mux)
{
	PurpleMediaBackendFs2Stream *stream;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), FALSE);
	stream = get_stream(PURPLE_MEDIA_BACKEND_FS2(self),
			sess_id, participant);

	if (stream != NULL &&
		g_object_class_find_property (G_OBJECT_GET_CLASS (stream->stream),
			"send-rtcp-mux") != NULL) {
		g_object_set (stream->stream, "send-rtcp-mux", send_rtcp_mux, NULL);
		return TRUE;
	}

	return FALSE;
}
#endif /* USE_VV */
