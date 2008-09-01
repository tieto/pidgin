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
#include <gst/farsight/fs-stream.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define PURPLE_TYPE_MEDIA            (purple_media_get_type())
#define PURPLE_MEDIA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MEDIA, PurpleMedia))
#define PURPLE_MEDIA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MEDIA, PurpleMediaClass))
#define PURPLE_IS_MEDIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MEDIA))
#define PURPLE_IS_MEDIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_MEDIA))
#define PURPLE_MEDIA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MEDIA, PurpleMediaClass))

/** @copydoc _PurpleMedia */
typedef struct _PurpleMedia PurpleMedia;
/** @copydoc _PurpleMediaClass */
typedef struct _PurpleMediaClass PurpleMediaClass;
/** @copydoc _PurpleMediaPrivate */
typedef struct _PurpleMediaPrivate PurpleMediaPrivate;
/** @copydoc _PurpleMediaSession */
typedef struct _PurpleMediaSession PurpleMediaSession;

#else

typedef void PurpleMedia;

#endif /* USE_VV */

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Gets the media class's GType
 *
 * @return The media class's GType.
 */
GType purple_media_get_type(void);

/**************************************************************************/
/** @name Media Utility Functions                                         */
/**************************************************************************/
/*@{*/

/**
 * Converts a PurpleMediaSessionType to an FsMediaType.
 *
 * @param type The type to derive FsMediaType from
 *
 * @return The FsMediaType derived from type
 */
FsMediaType purple_media_to_fs_media_type(PurpleMediaSessionType type);

/**
 * Converts a PurpleMediaSessionType to an FsStreamDirection.
 *
 * @param type The type to derive FsMediaType from
 *
 * @return The FsMediaDirection derived from type
 */
FsStreamDirection purple_media_to_fs_stream_direction(PurpleMediaSessionType type);

/**
 * Converts an FsMediaType and FsStreamDirection into a PurpleMediaSessionType.
 *
 * @param type The type used to construct PurpleMediaSessionType
 * @param direction The direction used to construct PurpleMediaSessionType
 *
 * @return The PurpleMediaSessionType constructed
 */
PurpleMediaSessionType purple_media_from_fs(FsMediaType type, FsStreamDirection direction);

/*@}*/

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
 * Sets the sink on a session.
 *
 * @param media The media object the session is in.
 * @param sess_id The session id of the session to set the sink on.
 * @param sink The source to set the session sink to.
 */
void purple_media_set_sink(PurpleMedia *media, const gchar *sess_id, GstElement *sink);

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
 * Gets the sink from a session
 *
 * @param media The media object the session is in.
 * @param sess_id The session id of the session to get the source from.
 *
 * @return The sink retrieved.
 */
GstElement *purple_media_get_sink(PurpleMedia *media, const gchar *sess_id);

/**
 * Gets the pipeline from the media session.
 *
 * @param media The media session to retrieve the pipeline from.
 *
 * @return The pipeline retrieved.
 */
GstElement *purple_media_get_pipeline(PurpleMedia *media);

/**
 * Gets the connection the media session is associated with.
 *
 * @param media The media object to retrieve the connection from.
 *
 * @return The retreived connection.
 */
PurpleConnection *purple_media_get_connection(PurpleMedia *media);

/**
 * Gets the screenname of the remote user.
 *
 * @param media The media object to retrieve the remote user from.
 *
 * @return The retrieved screenname.
 */
char *purple_media_get_screenname(PurpleMedia *media);

/**
 * Set the media session to the ready state.
 *
 * @param media The media object to set the state on.
 */
void purple_media_ready(PurpleMedia *media);

/**
 * Set the media session to the wait state.
 *
 * @param media The media object to set the state on.
 */
void purple_media_wait(PurpleMedia *media);

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
 * Set the media session to the got_request state.
 *
 * @param media The media object to set the state on.
 */
void purple_media_got_request(PurpleMedia *media);

/**
 * Set the media session to the got_hangup state.
 *
 * @param media The media object to set the state on.
 */
void purple_media_got_hangup(PurpleMedia *media);

/**
 * Set the media session to the got_accept state.
 *
 * @param media The media object to set the state on.
 */
void purple_media_got_accept(PurpleMedia *media);

/**
 * Enumerates a list of devices.
 *
 * @param element The plugin from which to enumerate devices.
 *
 * @return The list of enumerated devices.
 */
GList *purple_media_get_devices(GstElement *element);

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
 * Gets the negotiated codecs from a session.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to get the negotiated codecs from.
 *
 * @return The retreieved codecs.
 */
GList *purple_media_get_negotiated_codecs(PurpleMedia *media, const gchar *sess_id);

/**
 * Gets the local codecs from a session.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to get the local codecs from.
 *
 * @return The retreieved codecs.
 */
GList *purple_media_get_local_codecs(PurpleMedia *media, const gchar *sess_id);

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
 * Gets the active local candidate for the stream.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to find the stream in.
 * @param name The name of the remote user to get the active candidate from.
 *
 * @return The active candidate retrieved.
 */
FsCandidate *purple_media_get_local_candidate(PurpleMedia *media, const gchar *sess_id, const gchar *name);

/**
 * Gets the active remote candidate for the stream.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to find the stream in.
 * @param name The name of the remote user to get the remote candidate from.
 *
 * @return The remote candidate retrieved.
 */
FsCandidate *purple_media_get_remote_candidate(PurpleMedia *media, const gchar *sess_id, const gchar *name);

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
 * Returns whether or not the candidates for a remote user are prepared
 *
 * @param media The media object to find the remote user in.
 * @param name The remote user to check for.
 *
 * @return @c TRUE All streams for the remote user have candidates prepared, @c FALSE otherwise.
 */
gboolean purple_media_candidates_prepared(PurpleMedia *media, const gchar *name);

/**
 * Sets the send codec for the a session.
 *
 * @param media The media object to find the session in.
 * @param sess_id The session id of the session to set the codec for.
 * @param codec The codec to set the session to stream.
 *
 * @return @c TRUE The codec was successfully changed, or @c FALSE otherwise.
 */
gboolean purple_media_set_send_codec(PurpleMedia *media, const gchar *sess_id, FsCodec *codec);

/**
 * Mutes or unmutes all the audio local audio sources.
 *
 * @param media The media object to mute or unmute
 * @param active @c TRUE to mutes all of the local audio sources, or @c FALSE to unmute.
 */
void purple_media_mute(PurpleMedia *media, gboolean active);

#ifdef __cplusplus
}
#endif

G_END_DECLS

#endif  /* USE_VV */


#endif  /* __MEDIA_H_ */
