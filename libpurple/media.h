/**
 * @file media.h Media API
 * @ingroup core
 *
 * purple
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
#include <gst/farsight/fs-session.h>
#include <glib.h>
#include <glib-object.h>

#include "connection.h"

G_BEGIN_DECLS

#define PURPLE_TYPE_MEDIA            (purple_media_get_type())
#define PURPLE_MEDIA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MEDIA, PurpleMedia))
#define PURPLE_MEDIA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MEDIA, PurpleMediaClass))
#define PURPLE_IS_MEDIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MEDIA))
#define PURPLE_IS_MEDIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_MEDIA))
#define PURPLE_MEDIA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MEDIA, PurpleMediaClass))

typedef struct _PurpleMedia PurpleMedia;
typedef struct _PurpleMediaClass PurpleMediaClass;
typedef struct _PurpleMediaPrivate PurpleMediaPrivate;
typedef struct _PurpleMediaSession PurpleMediaSession;

typedef enum {
	PURPLE_MEDIA_NONE	= 0,
	PURPLE_MEDIA_RECV_AUDIO = 1 << 0,
	PURPLE_MEDIA_SEND_AUDIO = 1 << 1,
	PURPLE_MEDIA_RECV_VIDEO = 1 << 2,
	PURPLE_MEDIA_SEND_VIDEO = 1 << 3,
	PURPLE_MEDIA_AUDIO = PURPLE_MEDIA_RECV_AUDIO | PURPLE_MEDIA_SEND_AUDIO,
	PURPLE_MEDIA_VIDEO = PURPLE_MEDIA_RECV_VIDEO | PURPLE_MEDIA_SEND_VIDEO
} PurpleMediaStreamType;

struct _PurpleMediaClass
{
	GObjectClass parent_class;
};

struct _PurpleMedia
{
	GObject parent;
	PurpleMediaPrivate *priv;
};

GType purple_media_get_type(void);

FsMediaType purple_media_to_fs_media_type(PurpleMediaStreamType type);
FsStreamDirection purple_media_to_fs_stream_direction(PurpleMediaStreamType type);
PurpleMediaStreamType purple_media_from_fs(FsMediaType type, FsStreamDirection direction);

GList *purple_media_get_session_names(PurpleMedia *media);

void purple_media_get_elements(PurpleMedia *media, GstElement **audio_src, GstElement **audio_sink,
						  GstElement **video_src, GstElement **video_sink);

void purple_media_set_src(PurpleMedia *media, const gchar *sess_id, GstElement *src);
void purple_media_set_sink(PurpleMedia *media, const gchar *sess_id, GstElement *src);

GstElement *purple_media_get_src(PurpleMedia *media, const gchar *sess_id);
GstElement *purple_media_get_sink(PurpleMedia *media, const gchar *sess_id);

GstElement *purple_media_get_pipeline(PurpleMedia *media);

PurpleConnection *purple_media_get_connection(PurpleMedia *media);
const char *purple_media_get_screenname(PurpleMedia *media);
void purple_media_ready(PurpleMedia *media);
void purple_media_wait(PurpleMedia *media);
void purple_media_accept(PurpleMedia *media);
void purple_media_reject(PurpleMedia *media);
void purple_media_hangup(PurpleMedia *media);
void purple_media_got_hangup(PurpleMedia *media);
void purple_media_got_accept(PurpleMedia *media);

gchar *purple_media_get_device_name(GstElement *element, 
										  GValue *device);

GList *purple_media_get_devices(GstElement *element);
void purple_media_element_set_device(GstElement *element, GValue *device);
void purple_media_element_set_device_from_name(GstElement *element,
											   const gchar *name);
GValue *purple_media_element_get_device(GstElement *element);
GstElement *purple_media_get_element(const gchar *factory_name);

void purple_media_audio_init_src(GstElement **sendbin,
                                 GstElement **sendlevel);
void purple_media_video_init_src(GstElement **sendbin);

void purple_media_audio_init_recv(GstElement **recvbin, GstElement **recvlevel);
void purple_media_video_init_recv(GstElement **sendbin);

gboolean purple_media_add_stream(PurpleMedia *media, const gchar *sess_id, const gchar *who,
			     PurpleMediaStreamType type, const gchar *transmitter);
void purple_media_remove_stream(PurpleMedia *media, const gchar *sess_id, const gchar *who);

PurpleMediaStreamType purple_media_get_session_type(PurpleMedia *media, const gchar *sess_id);

GList *purple_media_get_negotiated_codecs(PurpleMedia *media, const gchar *sess_id);

GList *purple_media_get_local_codecs(PurpleMedia *media, const gchar *sess_id);
void purple_media_add_remote_candidates(PurpleMedia *media, const gchar *sess_id,
					const gchar *name, GList *remote_candidates);
GList *purple_media_get_local_candidates(PurpleMedia *media, const gchar *sess_id, const gchar *name);
FsCandidate *purple_media_get_local_candidate(PurpleMedia *media, const gchar *sess_id, const gchar *name);
FsCandidate *purple_media_get_remote_candidate(PurpleMedia *media, const gchar *sess_id, const gchar *name);
void purple_media_set_remote_codecs(PurpleMedia *media, const gchar *sess_id, const gchar *name, GList *codecs);

G_END_DECLS

#endif  /* USE_VV */


#endif  /* __MEDIA_H_ */
