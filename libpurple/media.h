/**
 * @file media.h Media API
 * @ingroup core
 */

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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __MEDIA_H_
#define __MEDIA_H_

#ifdef USE_VV

#include <gst/gst.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define PURPLE_TYPE_MEDIA            (purple_media_get_type())
#define PURPLE_TYPE_MEDIA_CANDIDATE  (purple_media_candidate_get_type())
#define PURPLE_TYPE_MEDIA_CODEC      (purple_media_codec_get_type())
#define PURPLE_MEDIA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MEDIA, PurpleMedia))
#define PURPLE_MEDIA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MEDIA, PurpleMediaClass))
#define PURPLE_IS_MEDIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MEDIA))
#define PURPLE_IS_MEDIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_MEDIA))
#define PURPLE_MEDIA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MEDIA, PurpleMediaClass))

#define PURPLE_MEDIA_TYPE_STATE_CHANGED	(purple_media_state_changed_get_type())

/** @copydoc _PurpleMedia */
typedef struct _PurpleMedia PurpleMedia;
/** @copydoc _PurpleMediaClass */
typedef struct _PurpleMediaClass PurpleMediaClass;
/** @copydoc _PurpleMediaPrivate */
typedef struct _PurpleMediaPrivate PurpleMediaPrivate;
/** @copydoc _PurpleMediaCandidate */
typedef struct _PurpleMediaCandidate PurpleMediaCandidate;
/** @copydoc _PurpleMediaCodec */
typedef struct _PurpleMediaCodec PurpleMediaCodec;
/** @copydoc _PurpleMediaCodecParameter */
typedef struct _PurpleMediaCodecParameter PurpleMediaCodecParameter;

#else

typedef void PurpleMedia;

#endif /* USE_VV */

/** Media caps */
typedef enum {
	PURPLE_MEDIA_CAPS_NONE = 0,
	PURPLE_MEDIA_CAPS_AUDIO = 1,
	PURPLE_MEDIA_CAPS_AUDIO_SINGLE_DIRECTION = 1 << 1,
	PURPLE_MEDIA_CAPS_VIDEO = 1 << 2,
	PURPLE_MEDIA_CAPS_VIDEO_SINGLE_DIRECTION = 1 << 3,
	PURPLE_MEDIA_CAPS_AUDIO_VIDEO = 1 << 4,
	PURPLE_MEDIA_CAPS_MODIFY_SESSION = 1 << 5,
	PURPLE_MEDIA_CAPS_CHANGE_DIRECTION = 1 << 6,
} PurpleMediaCaps;

/** Media session types */
typedef enum {
	PURPLE_MEDIA_NONE	= 0,
	PURPLE_MEDIA_RECV_AUDIO = 1 << 0,
	PURPLE_MEDIA_SEND_AUDIO = 1 << 1,
	PURPLE_MEDIA_RECV_VIDEO = 1 << 2,
	PURPLE_MEDIA_SEND_VIDEO = 1 << 3,
	PURPLE_MEDIA_AUDIO = PURPLE_MEDIA_RECV_AUDIO | PURPLE_MEDIA_SEND_AUDIO,
	PURPLE_MEDIA_VIDEO = PURPLE_MEDIA_RECV_VIDEO | PURPLE_MEDIA_SEND_VIDEO
} PurpleMediaSessionType;

/** Media state-changed types */
typedef enum {
	PURPLE_MEDIA_STATE_CHANGED_NEW = 0,
	PURPLE_MEDIA_STATE_CHANGED_CONNECTED,
	PURPLE_MEDIA_STATE_CHANGED_REJECTED,	/** Local user rejected the stream. */
	PURPLE_MEDIA_STATE_CHANGED_HANGUP,	/** Local user hung up the stream */
	PURPLE_MEDIA_STATE_CHANGED_END,
} PurpleMediaStateChangedType;

typedef enum {
	PURPLE_MEDIA_CANDIDATE_TYPE_HOST,
	PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX,
	PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX,
	PURPLE_MEDIA_CANDIDATE_TYPE_RELAY,
	PURPLE_MEDIA_CANDIDATE_TYPE_MULTICAST,
} PurpleMediaCandidateType;

typedef enum {
	PURPLE_MEDIA_COMPONENT_NONE = 0,
	PURPLE_MEDIA_COMPONENT_RTP = 1,
	PURPLE_MEDIA_COMPONENT_RTCP = 2,
} PurpleMediaComponentType;

typedef enum {
	PURPLE_MEDIA_NETWORK_PROTOCOL_UDP,
	PURPLE_MEDIA_NETWORK_PROTOCOL_TCP,
} PurpleMediaNetworkProtocol;

#ifdef USE_VV

/** The media class */
struct _PurpleMediaClass
{
	GObjectClass parent_class;     /**< The parent class. */
};

/** The media class's private data */
struct _PurpleMedia
{
	GObject parent;                /**< The parent of this object. */
	PurpleMediaPrivate *priv;      /**< The private data of this object. */
};

struct _PurpleMediaCandidate
{
	const gchar *foundation;
	guint component_id;
	const gchar *ip;
	guint16 port;
	const gchar *base_ip;
	guint16 base_port;
	PurpleMediaNetworkProtocol proto;
	guint32 priority;
	PurpleMediaCandidateType type;
	const gchar *username;
	const gchar *password;
	guint ttl;
};

struct _PurpleMediaCodecParameter
{
	gchar *name;
	gchar *value;
};

struct _PurpleMediaCodec
{
	gint id;
	char *encoding_name;
	PurpleMediaSessionType media_type;
	guint clock_rate;
	guint channels;
	GList *optional_params;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Gets the media class's GType
 *
 * @return The media class's GType.
 */
GType purple_media_get_type(void);

/**
 * Gets the type of the state-changed enum
 *
 * @return The state-changed enum's GType
 */
GType purple_media_state_changed_get_type(void);

/**
 * Gets the type of the media candidate structure.
 *
 * @return The media canditate's GType
 */
GType purple_media_candidate_get_type(void);

/**
 * Creates a PurpleMediaCandidate instance.
 *
 * @param foundation The foundation of the candidate.
 * @param component_id The component this candidate is for.
 * @param type The type of candidate.
 * @param proto The protocol this component is for.
 * @param ip The IP address of this component.
 * @param port The network port.
 *
 * @return The newly created PurpleMediaCandidate instance.
 */
PurpleMediaCandidate *purple_media_candidate_new(
		const gchar *foundation, guint component_id,
		PurpleMediaCandidateType type,
		PurpleMediaNetworkProtocol proto,
		const gchar *ip, guint port);

/**
 * Copies a PurpleMediaCandidate instance.
 *
 * @param candidate The candidate to copy
 *
 * @return The newly created PurpleMediaCandidate copy.
 */
PurpleMediaCandidate *purple_media_candidate_copy(
		PurpleMediaCandidate *candidate);

/**
 * Copies a GList of PurpleMediaCandidate and its contents.
 *
 * @param candidates The list of candidates to be copied.
 *
 * @return The copy of the GList.
 */
GList *purple_media_candidate_list_copy(GList *candidates);

/**
 * Frees a GList of PurpleMediaCandidate and its contents.
 *
 * @param candidates The list of candidates to be freed.
 */
void purple_media_candidate_list_free(GList *candidates);

/**
 * Gets the type of the media codec structure.
 *
 * @return The media codec's GType
 */
GType purple_media_codec_get_type(void);

/**
 * Creates a new PurpleMediaCodec instance.
 *
 * @param id Codec identifier.
 * @param encoding_name Name of the media type this encodes.
 * @param media_type PurpleMediaSessionType of this codec.
 * @param clock_rate The clock rate this codec encodes at, if applicable.
 *
 * @return The newly created PurpleMediaCodec.
 */
PurpleMediaCodec *purple_media_codec_new(int id, const char *encoding_name,
		PurpleMediaSessionType media_type, guint clock_rate);

/**
 * Copies a PurpleMediaCodec instance.
 *
 * @param codec The codec to copy
 *
 * @return The newly created PurpleMediaCodec copy.
 */
PurpleMediaCodec *purple_media_codec_copy(PurpleMediaCodec *codec);

/**
 * Creates a string representation of the codec.
 *
 * @param codec The codec to create the string of.
 *
 * @return The new string representation.
 */
gchar *purple_media_codec_to_string(const PurpleMediaCodec *codec);

/**
 * Adds an optional parameter to the codec.
 *
 * @param codec The codec to add the parameter to.
 * @param name The name of the parameter to add.
 * @param value The value of the parameter to add.
 */
void purple_media_codec_add_optional_parameter(PurpleMediaCodec *codec,
		const gchar *name, const gchar *value);

/**
 * Removes an optional parameter from the codec.
 *
 * @param codec The codec to remove the parameter from.
 * @param param A pointer to the parameter to remove.
 */
void purple_media_codec_remove_optional_parameter(PurpleMediaCodec *codec,
		PurpleMediaCodecParameter *param);

/**
 * Gets an optional parameter based on the values given.
 *
 * @param codec The codec to find the parameter in.
 * @param name The name of the parameter to search for.
 * @param value The value to search for or NULL.
 *
 * @return The value found or NULL.
 */
PurpleMediaCodecParameter *purple_media_codec_get_optional_parameter(
		PurpleMediaCodec *codec, const gchar *name,
		const gchar *value);

/**
 * Copies a GList of PurpleMediaCodec and its contents.
 *
 * @param codecs The list of codecs to be copied.
 *
 * @return The copy of the GList.
 */
GList *purple_media_codec_list_copy(GList *codecs);

/**
 * Frees a GList of PurpleMediaCodec and its contents.
 *
 * @param codecs The list of codecs to be freed.
 */
void purple_media_codec_list_free(GList *codecs);

/**
 * Combines all the separate session types into a single PurpleMediaSessionType.
 *
 * @param media The media session to retrieve session types from.
 *
 * @return Combined type.
 */
PurpleMediaSessionType purple_media_get_overall_type(PurpleMedia *media);

/**
 * Gets a list of session names.
 *
 * @param media The media session to retrieve session names from.
 *
 * @return GList of session names.
 */
GList *purple_media_get_session_names(PurpleMedia *media);

/**
 * Gets an audio and video source and sink from the media session.
 *
 * Retrieves the first of each element in the media session.
 *
 * @param media The media session to retreive the sources and sinks from.
 * @param audio_src Set to the audio source.
 * @param audio_sink Set to the audio sink.
 * @param video_src Set to the video source.
 * @param video_sink Set to the video sink.
 */
void purple_media_get_elements(PurpleMedia *media,
			       GstElement **audio_src, GstElement **audio_sink,
			       GstElement **video_src, GstElement **video_sink);

/**
 * Sets the source on a session.
 *
 * @param media The media object the session is in.
 * @param sess_id The session id of the session to set the source on.
 * @param src The source to set the session source to.
 */
void purple_media_set_src(PurpleMedia *media, const gchar *sess_id, GstElement *src);

/**
 * Sets the sink on a stream.
 *
 * @param media The media object the session is in.
 * @param sess_id The session id the stream belongs to.
 * @param sess_id The participant the stream is associated with.
 * @param sink The source to set the session sink to.
 */
void purple_media_set_sink(PurpleMedia *media, const gchar *sess_id,
		const gchar *participant, GstElement *sink);

/**
 * Gets the source from a session
 *
 * @param media The media object the session is in.
 * @param sess_id The session id of the session to get the source from.
 *
 * @return The source retrieved.
 */
GstElement *purple_media_get_src(PurpleMedia *media, const gchar *sess_id);

/**
 * Gets the sink from a stream
 *
 * @param media The media object the session is in.
 * @param sess_id The session id the stream belongs to.
 * @param participant The participant the stream is associated with.
 *
 * @return The sink retrieved.
 */
GstElement *purple_media_get_sink(PurpleMedia *media, const gchar *sess_id, const gchar *participant);

/**
 * Gets the pipeline from the media session.
 *
 * @param media The media session to retrieve the pipeline from.
 *
 * @return The pipeline retrieved.
 */
GstElement *purple_media_get_pipeline(PurpleMedia *media);

/**
 * Gets the PurpleConnection this media session is on.
 *
 * @param media The media session to retrieve the connection from.
 *
 * @return The connection retrieved.
 */
PurpleConnection *purple_media_get_connection(PurpleMedia *media);

/**
 * Gets the prpl data from the media session.
 *
 * @param media The media session to retrieve the prpl data from.
 *
 * @return The prpl data retrieved.
 */
gpointer purple_media_get_prpl_data(PurpleMedia *media);

/**
 * Sets the prpl data on the media session.
 *
 * @param media The media session to set the prpl data on.
 * @param prpl_data The data to set on the media session.
 */
void purple_media_set_prpl_data(PurpleMedia *media, gpointer prpl_data);

/**
 * Signals an error in the media session.
 *
 * @param media The media object to set the state on.
 * @param error The format of the error message to send in the signal.
 * @param ... The arguments to plug into the format.
 */
void purple_media_error(PurpleMedia *media, const gchar *error, ...);

/**
 * Set the media session to the accepted state.
 *
 * @param media The media object to set the state on.
 */
void purple_media_accept(PurpleMedia *media);

/**
 * Set the media session to the rejected state.
 *
 * @param media The media object to set the state on.
 */
void purple_media_reject(PurpleMedia *media);

/**
 * Set the media session to the hangup state.
 *
 * @param media The media object to set the state on.
 */
void purple_media_hangup(PurpleMedia *media);

/**
 * Ends all streams that match the given parameters
 *
 * @param media The media object with which to end streams.
 * @param session_id The session to end streams on.
 * @param participant The participant to end streams with.
 */
void purple_media_end(PurpleMedia *media, const gchar *session_id,
		const gchar *participant);

/**
 * Enumerates a list of devices.
 *
 * @param plugin The name of the GStreamer plugin from which to enumerate devices.
 *
 * @return The list of enumerated devices.
 */
GList *purple_media_get_devices(const gchar *plugin);

/**
 * Gets the device the plugin is currently set to.
 *
 * @param element The plugin to retrieve the device from.
 *
 * @return The device retrieved.
 */
gchar *purple_media_element_get_device(GstElement *element);

/**
 * Creates a default audio source.
 *
 * @param sendbin Set to the newly created audio source.
 * @param sendlevel Set to the newly created level within the audio source.
 */
void purple_media_audio_init_src(GstElement **sendbin,
                                 GstElement **sendlevel);

/**
 * Creates a default video source.
 *
 * @param sendbin Set to the newly created video source.
 */
void purple_media_video_init_src(GstElement **sendbin);

/**
 * Creates a default audio sink.
 *
 * @param recvbin Set to the newly created audio sink.
 * @param recvlevel Set to the newly created level within the audio sink.
 */
void purple_media_audio_init_recv(GstElement **recvbin, GstElement **recvlevel);

/**
 * Creates a default video sink.
 *
 * @param sendbin Set to the newly created video sink.
 */
void purple_media_video_init_recv(GstElement **sendbin);

/**
 * Adds a stream to a session.
 *
 * It only adds a stream to one audio session or video session as
 * the @c sess_id must be unique between sessions.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to add the stream to.
 * @param who The name of the remote user to add the stream for.
 * @param type The type of stream to create.
 * @param transmitter The transmitter to use for the stream.
 * @param num_params The number of parameters to pass to Farsight.
 * @param params The parameters to pass to Farsight.
 *
 * @return @c TRUE The stream was added successfully, @c FALSE otherwise.
 */
gboolean purple_media_add_stream(PurpleMedia *media, const gchar *sess_id, const gchar *who,
		PurpleMediaSessionType type, const gchar *transmitter,
		guint num_params, GParameter *params);

/**
 * Removes a stream from a session.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to remove the stream from.
 * @param who The name of the remote user to remove the stream from.
 */
void purple_media_remove_stream(PurpleMedia *media, const gchar *sess_id, const gchar *who);

/**
 * Gets the session type from a session
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to get the type from.
 *
 * @return The retreived session type.
 */
PurpleMediaSessionType purple_media_get_session_type(PurpleMedia *media, const gchar *sess_id);

/**
 * Gets the codecs from a session.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to get the codecs from.
 *
 * @return The retreieved codecs.
 */
GList *purple_media_get_codecs(PurpleMedia *media, const gchar *sess_id);

/**
 * Adds remote candidates to the stream.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session find the stream in.
 * @param name The name of the remote user to add the candidates for.
 * @param remote_candidates The remote candidates to add.
 */
void purple_media_add_remote_candidates(PurpleMedia *media,
					const gchar *sess_id,
					const gchar *name,
					GList *remote_candidates);

/**
 * Gets the local candidates from a stream.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to find the stream in.
 * @param name The name of the remote user to get the candidates from.
 */
GList *purple_media_get_local_candidates(PurpleMedia *media,
					 const gchar *sess_id,
					 const gchar *name);

/**
 * Gets the active local candidates for the stream.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to find the stream in.
 * @param name The name of the remote user to get the active candidate from.
 *
 * @return The active candidates retrieved.
 */
GList *purple_media_get_active_local_candidates(PurpleMedia *media,
		const gchar *sess_id, const gchar *name);

/**
 * Gets the active remote candidates for the stream.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to find the stream in.
 * @param name The name of the remote user to get the remote candidate from.
 *
 * @return The remote candidates retrieved.
 */
GList *purple_media_get_active_remote_candidates(PurpleMedia *media,
		const gchar *sess_id, const gchar *name);

/**
 * Gets remote candidates from the stream.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session find the stream in.
 * @param name The name of the remote user to get the candidates from.
 *
 * @return @c TRUE The codecs were set successfully, or @c FALSE otherwise.
 */
gboolean purple_media_set_remote_codecs(PurpleMedia *media, const gchar *sess_id,
					const gchar *name, GList *codecs);

/**
 * Returns whether or not the candidates for set of streams are prepared
 *
 * @param media The media object to find the remote user in.
 * @param session_id The session id of the session to check.
 * @param participant The remote user to check for.
 *
 * @return @c TRUE All streams for the given session_id/participant combination have candidates prepared, @c FALSE otherwise.
 */
gboolean purple_media_candidates_prepared(PurpleMedia *media,
		const gchar *session_id, const gchar *participant);

/**
 * Sets the send codec for the a session.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to set the codec for.
 * @param codec The codec to set the session to stream.
 *
 * @return @c TRUE The codec was successfully changed, or @c FALSE otherwise.
 */
gboolean purple_media_set_send_codec(PurpleMedia *media, const gchar *sess_id, PurpleMediaCodec *codec);

/**
 * Gets whether a session's codecs are ready to be used.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to check.
 *
 * @return @c TRUE The codecs are ready, or @c FALSE otherwise.
 */
gboolean purple_media_codecs_ready(PurpleMedia *media, const gchar *sess_id);

/**
 * Gets whether a streams selected have been accepted.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to check.
 * @param participant The participant to check.
 *
 * @return @c TRUE The selected streams have been accepted, or @c FALSE otherwise.
 */
gboolean purple_media_accepted(PurpleMedia *media, const gchar *sess_id,
		const gchar *participant);

/**
 * Mutes or unmutes all the audio local audio sources.
 *
 * @param media The media object to mute or unmute
 * @param active @c TRUE to mutes all of the local audio sources, or @c FALSE to unmute.
 */
void purple_media_mute(PurpleMedia *media, gboolean active);

/**
 * Sets the input volume of all the selected sessions.
 *
 * @param media The media object the sessions are in.
 * @param session_id The session to select (if any).
 * @param level The level to set the volume to.
 */
void purple_media_set_input_volume(PurpleMedia *media, const gchar *session_id, double level);

/**
 * Sets the output volume of all the selected streams.
 *
 * @param media The media object the streams are in.
 * @param session_id The session to limit the streams to (if any).
 * @param participant The participant to limit the streams to (if any).
 * @param level The level to set the volume to.
 */
void purple_media_set_output_volume(PurpleMedia *media, const gchar *session_id,
		const gchar *participant, double level);

gulong purple_media_set_output_window(PurpleMedia *media,
		const gchar *session_id, const gchar *participant,
		gulong window_id);

void purple_media_remove_output_windows(PurpleMedia *media);

GstElement *purple_media_get_tee(PurpleMedia *media,
		const gchar *session_id, const gchar *participant);

#ifdef __cplusplus
}
#endif

G_END_DECLS

#endif  /* USE_VV */


#endif  /* __MEDIA_H_ */
