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

#ifndef _PURPLE_MEDIA_H_
#define _PURPLE_MEDIA_H_
/**
 * SECTION:media
 * @section_id: libpurple-media
 * @short_description: <filename>media.h</filename>
 * @title: Media Object API
 */

#include <glib.h>
#include <glib-object.h>

typedef struct _PurpleMedia PurpleMedia;

#include "media/candidate.h"
#include "media/codec.h"
#include "media/enum-types.h"

#define PURPLE_TYPE_MEDIA            (purple_media_get_type())
#define PURPLE_MEDIA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MEDIA, PurpleMedia))
#define PURPLE_MEDIA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MEDIA, PurpleMediaClass))
#define PURPLE_IS_MEDIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MEDIA))
#define PURPLE_IS_MEDIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_MEDIA))
#define PURPLE_MEDIA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MEDIA, PurpleMediaClass))

#include "signals.h"
#include "util.h"

#ifdef USE_VV

typedef struct _PurpleMediaClass    PurpleMediaClass;
typedef struct _PurpleMediaPrivate  PurpleMediaPrivate;

/**
 * PurpleMedia:
 *
 * The media instance
 */
struct _PurpleMedia
{
	GObject parent;

	/*< private >*/
	PurpleMediaPrivate *priv;
};

/**
 * PurpleMediaClass:
 *
 * The media class
 */
struct _PurpleMediaClass
{
	GObjectClass parent_class;

	/*< private >*/
	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

#endif

G_BEGIN_DECLS

/**
 * purple_media_get_type:
 *
 * Gets the media class's GType
 *
 * Returns: The media class's GType.
 */
GType purple_media_get_type(void);

/**
 * purple_media_get_session_ids:
 * @media: The media session from which to retrieve session IDs.
 *
 * Gets a list of session IDs.
 *
 * Returns: GList of session IDs. The caller must free the list.
 */
GList *purple_media_get_session_ids(PurpleMedia *media);

/**
 * purple_media_get_account:
 * @media: The media session to retrieve the account from.
 *
 * Gets the PurpleAccount this media session is on.
 *
 * Returns: The account retrieved.
 */
PurpleAccount *purple_media_get_account(PurpleMedia *media);

/**
 * purple_media_get_prpl_data:
 * @media: The media session to retrieve the protocol data from.
 *
 * Gets the protocol data from the media session.
 *
 * Returns: The protocol data retrieved.
 */
gpointer purple_media_get_prpl_data(PurpleMedia *media);

/**
 * purple_media_set_prpl_data:
 * @media: The media session to set the protocol data on.
 * @protocol_data: The data to set on the media session.
 *
 * Sets the protocol data on the media session.
 */
void purple_media_set_prpl_data(PurpleMedia *media, gpointer protocol_data);

/**
 * purple_media_error:
 * @media: The media object to set the state on.
 * @error: The format of the error message to send in the signal.
 * @...: The arguments to plug into the format.
 *
 * Signals an error in the media session.
 */
void purple_media_error(PurpleMedia *media, const gchar *error, ...);

/**
 * purple_media_end:
 * @media: The media object with which to end streams.
 * @session_id: The session to end streams on.
 * @participant: The participant to end streams with.
 *
 * Ends all streams that match the given parameters
 */
void purple_media_end(PurpleMedia *media, const gchar *session_id,
		const gchar *participant);

/**
 * purple_media_stream_info:
 * @media: The media instance to containing the stream to signal.
 * @type: The type of info being signaled.
 * @session_id: The id of the session of the stream being signaled.
 * @participant: The participant of the stream being signaled.
 * @local: TRUE if the info originated locally, FALSE if on the remote end.
 *
 * Signals different information about the given stream.
 */
void purple_media_stream_info(PurpleMedia *media, PurpleMediaInfoType type,
		const gchar *session_id, const gchar *participant,
		gboolean local);

/**
 * purple_media_set_params:
 * @media: The media object to set the parameters on.
 * @num_params: The number of parameters to pass
 * @params: Array of @c GParameter to pass
 *
 * Sets various optional parameters of the media call.
 *
 * Currently supported are:
 *   - "sdes-cname"    : The CNAME for the RTP sessions
 *   - "sdes-name"     : Real name used to describe the source in SDES messages
 *   - "sdes-tool"     : The TOOL to put in SDES messages
 *   - "sdes-email"    : Email address to put in SDES messages
 *   - "sdes-location" : The LOCATION to put in SDES messages
 *   - "sdes-note"     : The NOTE to put in SDES messages
 *   - "sdes-phone"    : The PHONE to put in SDES messages
 */
void purple_media_set_params(PurpleMedia *media,
		guint num_params, GParameter *params);

/**
 * purple_media_get_available_params:
 * @media: The media object
 *
 * Gets the list of optional parameters supported by the media backend.
 *
 * The list is owned by the #PurpleMedia internals and should NOT be freed.
 *
 * Returns: NULL-terminated array of names of supported parameters.
 */
const gchar **purple_media_get_available_params(PurpleMedia *media);

/**
 * purple_media_param_is_supported:
 * @media: The media object
 * @param: name of parameter
 *
 * Checks if given optional parameter is supported by the media backend.
 *
 * Returns: %TRUE if backend recognizes the parameter, %FALSE otherwise.
 */
gboolean purple_media_param_is_supported(PurpleMedia *media, const gchar *param);

/**
 * purple_media_add_stream:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session to add the stream to.
 * @who: The name of the remote user to add the stream for.
 * @type: The type of stream to create.
 * @initiator: Whether or not the local user initiated the stream.
 * @transmitter: The transmitter to use for the stream.
 * @num_params: The number of parameters to pass to Farsight.
 * @params: The parameters to pass to Farsight.
 *
 * Adds a stream to a session.
 *
 * It only adds a stream to one audio session or video session as
 * the @sess_id must be unique between sessions.
 *
 * Returns: %TRUE The stream was added successfully, %FALSE otherwise.
 */
gboolean purple_media_add_stream(PurpleMedia *media, const gchar *sess_id,
		const gchar *who, PurpleMediaSessionType type,
		gboolean initiator, const gchar *transmitter,
		guint num_params, GParameter *params);

/**
 * purple_media_get_session_type:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session to get the type from.
 *
 * Gets the session type from a session
 *
 * Returns: The retreived session type.
 */
PurpleMediaSessionType purple_media_get_session_type(PurpleMedia *media, const gchar *sess_id);

/**
 * purple_media_get_manager:
 * @media: The media object to get the manager instance from.
 *
 * Gets the PurpleMediaManager this media session is a part of.
 *
 * Returns: The PurpleMediaManager instance retrieved.
 */
struct _PurpleMediaManager *purple_media_get_manager(PurpleMedia *media);

/**
 * purple_media_get_codecs:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session to get the codecs from.
 *
 * Gets the codecs from a session.
 *
 * Returns: The retreieved codecs.
 */
GList *purple_media_get_codecs(PurpleMedia *media, const gchar *sess_id);

/**
 * purple_media_add_remote_candidates:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session find the stream in.
 * @participant: The name of the remote user to add the candidates for.
 * @remote_candidates: The remote candidates to add.
 *
 * Adds remote candidates to the stream.
 */
void purple_media_add_remote_candidates(PurpleMedia *media,
					const gchar *sess_id,
					const gchar *participant,
					GList *remote_candidates);

/**
 * purple_media_get_local_candidates:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session to find the stream in.
 * @participant: The name of the remote user to get the candidates from.
 *
 * Gets the local candidates from a stream.
 */
GList *purple_media_get_local_candidates(PurpleMedia *media,
					 const gchar *sess_id,
					 const gchar *participant);

/**
 * purple_media_get_active_local_candidates:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session to find the stream in.
 * @participant: The name of the remote user to get the active candidate
 *                    from.
 *
 * Gets the active local candidates for the stream.
 *
 * Returns: The active candidates retrieved.
 */
GList *purple_media_get_active_local_candidates(PurpleMedia *media,
		const gchar *sess_id, const gchar *participant);

/**
 * purple_media_get_active_remote_candidates:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session to find the stream in.
 * @participant: The name of the remote user to get the remote candidate
 *                    from.
 *
 * Gets the active remote candidates for the stream.
 *
 * Returns: The remote candidates retrieved.
 */
GList *purple_media_get_active_remote_candidates(PurpleMedia *media,
		const gchar *sess_id, const gchar *participant);

/**
 * purple_media_set_remote_codecs:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session find the stream in.
 * @participant: The name of the remote user to set the codecs for.
 * @codecs: The list of remote codecs to set.
 *
 * Sets remote codecs from the stream.
 *
 * Returns: %TRUE The codecs were set successfully, or %FALSE otherwise.
 */
gboolean purple_media_set_remote_codecs(PurpleMedia *media, const gchar *sess_id,
					const gchar *participant, GList *codecs);

/**
 * purple_media_candidates_prepared:
 * @media: The media object to find the remote user in.
 * @session_id: The session id of the session to check.
 * @participant: The remote user to check for.
 *
 * Returns whether or not the candidates for set of streams are prepared
 *
 * Returns: %TRUE All streams for the given session_id/participant combination have candidates prepared, %FALSE otherwise.
 */
gboolean purple_media_candidates_prepared(PurpleMedia *media,
		const gchar *session_id, const gchar *participant);

/**
 * purple_media_set_send_codec:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session to set the codec for.
 * @codec: The codec to set the session to stream.
 *
 * Sets the send codec for the a session.
 *
 * Returns: %TRUE The codec was successfully changed, or %FALSE otherwise.
 */
gboolean purple_media_set_send_codec(PurpleMedia *media, const gchar *sess_id, PurpleMediaCodec *codec);

/**
 * purple_media_set_encryption_parameters:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session to set parameters of.
 * @cipher: The cipher to use to encrypt our media in the session.
 * @auth: The algorithm to use to compute authentication codes for our media
 *        frames.
 * @key: The encryption key.
 * @key_len: Byte length of the encryption key.
 *
 * Sets the encryption parameters of our media in the session.
 */
gboolean purple_media_set_encryption_parameters(PurpleMedia *media,
		const gchar *sess_id, const gchar *cipher,
		const gchar *auth, const gchar *key, gsize key_len);

/**
 * purple_media_set_decryption_parameters:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session to set parameters of.
 * @participant: The participant of the session to set parameters of.
 * @cipher: The cipher to use to decrypt media coming from this session's
 *          participant.
 * @auth: The algorithm to use for authentication of the media coming from
 *        the session's participant.
 * @key: The decryption key.
 * @key_len: Byte length of the decryption key.
 *
 * Sets the decryption parameters for a session participant's media.
 */
gboolean purple_media_set_decryption_parameters(PurpleMedia *media,
		const gchar *sess_id, const gchar *participant,
		const gchar *cipher, const gchar *auth,
		const gchar *key, gsize key_len);

/**
 * purple_media_codecs_ready:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session to check.
 *
 * Gets whether a session's codecs are ready to be used.
 *
 * Returns: %TRUE The codecs are ready, or %FALSE otherwise.
 */
gboolean purple_media_codecs_ready(PurpleMedia *media, const gchar *sess_id);

/**
 * purple_media_set_send_rtcp_mux:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session find the stream in.
 * @participant: The name of the remote user to set the rtcp-mux for.
 * @send_rtcp_mux: Whether to enable the rtcp-mux option
 *
 * Sets the rtcp-mux option for the stream.
 *
 * Returns: %TRUE RTCP-Mux was set successfully, or %FALSE otherwise.
 */
gboolean purple_media_set_send_rtcp_mux(PurpleMedia *media,
		const gchar *sess_id, const gchar *participant, gboolean send_rtcp_mux);
/**
 * purple_media_is_initiator:
 * @media: The media instance to find the session in.
 * @sess_id: The session id of the session to check.
 * @participant: The participant of the stream to check.
 *
 * Gets whether the local user is the conference/session/stream's initiator.
 *
 * Returns: TRUE if the local user is the stream's initator, else FALSE.
 */
gboolean purple_media_is_initiator(PurpleMedia *media,
		const gchar *sess_id, const gchar *participant);

/**
 * purple_media_accepted:
 * @media: The media object to find the session in.
 * @sess_id: The session id of the session to check.
 * @participant: The participant to check.
 *
 * Gets whether a streams selected have been accepted.
 *
 * Returns: %TRUE The selected streams have been accepted, or %FALSE otherwise.
 */
gboolean purple_media_accepted(PurpleMedia *media, const gchar *sess_id,
		const gchar *participant);

/**
 * purple_media_set_input_volume:
 * @media: The media object the sessions are in.
 * @session_id: The session to select (if any).
 * @level: The level to set the volume to.
 *
 * Sets the input volume of all the selected sessions.
 */
void purple_media_set_input_volume(PurpleMedia *media, const gchar *session_id, double level);

/**
 * purple_media_set_output_volume:
 * @media: The media object the streams are in.
 * @session_id: The session to limit the streams to (if any).
 * @participant: The participant to limit the streams to (if any).
 * @level: The level to set the volume to.
 *
 * Sets the output volume of all the selected streams.
 */
void purple_media_set_output_volume(PurpleMedia *media, const gchar *session_id,
		const gchar *participant, double level);

/**
 * purple_media_set_output_window:
 * @media: The media instance to set the output window on.
 * @session_id: The session to set the output window on.
 * @participant: Optionally, the participant to set the output window on.
 * @window_id: The window id use for embedding the video in.
 *
 * Sets a video output window for the given session/stream.
 *
 * Returns: An id to reference the output window.
 */
gulong purple_media_set_output_window(PurpleMedia *media,
		const gchar *session_id, const gchar *participant,
		gulong window_id);

/**
 * purple_media_remove_output_windows:
 * @media: The instance to remove all output windows from.
 *
 * Removes all output windows from a given media session.
 */
void purple_media_remove_output_windows(PurpleMedia *media);

/**
 * purple_media_send_dtmf:
 * @media: The media instance to send a DTMF signal to.
 * @sess_id: The session id of the session to send the DTMF signal on.
 * @dtmf: The character representing the DTMF in the range [0-9#*A-D].
 * @volume: The power level expressed in dBm0 after dropping the sign in the
 *          range of 0 to 63.  A larger value represents a lower volume.
 * @duration: The duration of the tone in milliseconds.
 *
 * Sends a DTMF signal out-of-band.
 *
 * Returns: %TRUE DTMF sent successfully, or %FALSE otherwise.
 */
gboolean purple_media_send_dtmf(PurpleMedia *media, const gchar *session_id,
		gchar dtmf, guint8 volume, guint16 duration);

G_END_DECLS

#endif  /* _PURPLE_MEDIA_H_ */
