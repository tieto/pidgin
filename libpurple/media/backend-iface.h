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
 * SECTION:backend-iface
 * @section_id: libpurple-backend-iface
 * @short_description: <filename>media/backend-iface.h</filename>
 * @title: Interface for media backends
 */

#ifndef _MEDIA_BACKEND_IFACE_H_
#define _MEDIA_BACKEND_IFACE_H_

#include "codec.h"
#include "enum-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define PURPLE_TYPE_MEDIA_BACKEND		(purple_media_backend_get_type())
#define PURPLE_IS_MEDIA_BACKEND(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MEDIA_BACKEND))
#define PURPLE_MEDIA_BACKEND(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MEDIA_BACKEND, PurpleMediaBackend))
#define PURPLE_MEDIA_BACKEND_GET_INTERFACE(inst)(G_TYPE_INSTANCE_GET_INTERFACE((inst), PURPLE_TYPE_MEDIA_BACKEND, PurpleMediaBackendIface))

/**
 * PurpleMediaBackend:
 *
 * A placeholder to represent any media backend
 */
typedef struct _PurpleMediaBackend PurpleMediaBackend;

/**
 * PurpleMediaBackendIface:
 *
 * A structure to derive media backends from.
 */
typedef struct _PurpleMediaBackendIface PurpleMediaBackendIface;

struct _PurpleMediaBackendIface
{
	GTypeInterface parent_iface; /**< The parent iface class */

	/** Implementable functions called with purple_media_backend_* */
	gboolean (*add_stream) (PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *who,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter,
		guint num_params, GParameter *params);
	void (*add_remote_candidates) (PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *remote_candidates);
	gboolean (*codecs_ready) (PurpleMediaBackend *self,
		const gchar *sess_id);
	GList *(*get_codecs) (PurpleMediaBackend *self,
		const gchar *sess_id);
	GList *(*get_local_candidates) (PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant);
	gboolean (*set_remote_codecs) (PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *codecs);
	gboolean (*set_send_codec) (PurpleMediaBackend *self,
		const gchar *sess_id, PurpleMediaCodec *codec);
	void (*set_params) (PurpleMediaBackend *self,
		guint num_params, GParameter *params);
	const gchar **(*get_available_params) (void);
};

/**
 * purple_media_backend_get_type:
 *
 * Gets the media backend's GType.
 *
 * Returns: The media backend's GType.
 */
GType purple_media_backend_get_type(void);

/**
 * purple_media_backend_add_stream:
 * @self: The backend to add the stream to.
 * @sess_id: The session id of the stream to add.
 * @who: The remote participant of the stream to add.
 * @type: The media type and direction of the stream to add.
 * @initiator: True if the local user initiated the stream.
 * @transmitter: The string id of the tranmsitter to use.
 * @num_params: The number of parameters in the param parameter.
 * @params: The additional parameters to pass when creating the stream.
 *
 * Creates and adds a stream to the media backend.
 *
 * Returns: True if the stream was successfully created, othewise False.
 */
gboolean purple_media_backend_add_stream(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *who,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter,
		guint num_params, GParameter *params);

/**
 * purple_media_backend_add_remote_candidates:
 * @self: The backend the stream is in.
 * @sess_id: The session id associated with the stream.
 * @participant: The participant associated with the stream.
 * @remote_candidates: The list of remote candidates to add.
 *
 * Add remote candidates to a stream.
 */
void purple_media_backend_add_remote_candidates(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *remote_candidates);

/**
 * purple_media_backend_codecs_ready:
 * @self: The media backend the session is in.
 * @sess_id: The session id of the session to check.
 *
 * Get whether or not a session's codecs are ready.
 *
 * A codec is ready if all of the attributes and additional
 * parameters have been collected.
 *
 * Returns: True if the codecs are ready, otherwise False.
 */
gboolean purple_media_backend_codecs_ready(PurpleMediaBackend *self,
		const gchar *sess_id);

/**
 * purple_media_backend_get_codecs:
 * @self: The media backend the session is in.
 * @sess_id: The session id of the session to use.
 *
 * Gets the codec intersection list for a session.
 *
 * The intersection list consists of all codecs that are compatible
 * between the local and remote software.
 *
 * Returns: The codec intersection list.
 */
GList *purple_media_backend_get_codecs(PurpleMediaBackend *self,
		const gchar *sess_id);

/**
 * purple_media_backend_get_local_candidates:
 * @self: The media backend the stream is in.
 * @sess_id: The session id associated with the stream.
 * @particilant: The participant associated with the stream.
 *
 * Gets the list of local candidates for a stream.
 *
 * Returns: The list of local candidates.
 */
GList *purple_media_backend_get_local_candidates(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant);

/**
 * purple_media_backend_set_remote_codecs:
 * @self: The media backend the stream is in.
 * @sess_id: The session id the stream is associated with.
 * @participant: The participant the stream is associated with.
 * @codecs: The list of remote codecs to set.
 *
 * Sets the remote codecs on a stream.
 *
 * Returns: True if the remote codecs were set successfully, otherwise False.
 */
gboolean purple_media_backend_set_remote_codecs(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *codecs);

/**
 * purple_media_backend_set_send_codec:
 * @self: The media backend the session is in.
 * @sess_id: The session id of the session to set the codec for.
 * @codec: The codec to set.
 *
 * Sets which codec format to send media content in for a session.
 *
 * Returns: True if set successfully, otherwise False.
 */
gboolean purple_media_backend_set_send_codec(PurpleMediaBackend *self,
		const gchar *sess_id, PurpleMediaCodec *codec);

/**
 * purple_media_backend_set_params:
 * @self: The media backend to set the parameters on.
 * @num_params: The number of parameters to pass to backend
 * @params: Array of @c GParameter to pass to backend
 *
 * Sets various optional parameters of the media backend.
 */
void purple_media_backend_set_params(PurpleMediaBackend *self,
		guint num_params, GParameter *params);

/**
 * purple_media_backend_get_available_params:
 * @self: The media backend
 *
 * Gets the list of optional parameters supported by the media backend.
 * The list should NOT be freed.
 *
 * Returns: NULL-terminated array of names of supported parameters.
 */
const gchar **purple_media_backend_get_available_params(PurpleMediaBackend *self);

G_END_DECLS

#endif /* _MEDIA_BACKEND_IFACE_H_ */
