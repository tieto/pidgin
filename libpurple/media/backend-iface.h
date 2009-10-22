/**
 * @file backend-iface.h Interface for media backends
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
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

typedef struct _PurpleMediaBackend PurpleMediaBackend;
typedef struct _PurpleMediaBackendIface PurpleMediaBackendIface;

struct _PurpleMediaBackendIface
{
	GTypeInterface parent_iface;

	void (*do_action) (PurpleMediaBackend *self);
	gboolean (*add_stream) (PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *who,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter,
		guint num_params, GParameter *params);
	void (*add_remote_candidates) (PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *remote_candidates);
	GList *(*get_codecs) (PurpleMediaBackend *self,
		const gchar *sess_id);
	GList *(*get_local_candidates) (PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant);
	void (*set_remote_codecs) (PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *codecs);
	void (*set_send_codec) (PurpleMediaBackend *self,
		const gchar *sess_id, PurpleMediaCodec *codec);
};

GType purple_media_backend_get_type(void);

gboolean purple_media_backend_add_stream(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *who,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter,
		guint num_params, GParameter *params);
void purple_media_backend_add_remote_candidates(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *remote_candidates);
GList *purple_media_backend_get_codecs(PurpleMediaBackend *self,
		const gchar *sess_id);
GList *purple_media_backend_get_local_candidates(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant);
void purple_media_backend_set_remote_codecs(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *codecs);
void purple_media_backend_set_send_codec(PurpleMediaBackend *self,
		const gchar *sess_id, PurpleMediaCodec *codec);

G_END_DECLS

#endif /* _MEDIA_BACKEND_IFACE_H_ */
