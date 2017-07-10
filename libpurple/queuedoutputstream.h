/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PURPLE_QUEUED_OUTPUT_STREAM_H
#define _PURPLE_QUEUED_OUTPUT_STREAM_H
/**
 * SECTION:queuedoutputstream
 * @section_id: libpurple-queuedoutputstream
 * @short_description: GOutputStream for queuing data to output
 * @title: GOutputStream class
 *
 * A #PurpleQueuedOutputStream is a #GOutputStream which allows data to be
 * queued for outputting. It differs from a #GBufferedOutputStream in that
 * it allows for data to be queued while other operations are in progress.
 */

#include <gio/gio.h>

G_BEGIN_DECLS

#define PURPLE_TYPE_QUEUED_OUTPUT_STREAM		(purple_queued_output_stream_get_type())
#define PURPLE_QUEUED_OUTPUT_STREAM(o)			(G_TYPE_CHECK_INSTANCE_CAST((o), PURPLE_TYPE_QUEUED_OUTPUT_STREAM, PurpleQueuedOutputStream))
#define PURPLE_QUEUED_OUTPUT_STREAM_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), PURPLE_TYPE_QUEUED_OUTPUT_STREAM, PurpleQueuedOutputStreamClass))
#define PURPLE_IS_QUEUED_OUTPUT_STREAM(o)		(G_TYPE_CHECK_INSTANCE_TYPE((o), PURPLE_TYPE_QUEUED_OUTPUT_STREAM))
#define PURPLE_IS_QUEUED_OUTPUT_STREAM_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE((k), PURPLE_TYPE_QUEUED_OUTPUT_STREAM))
#define PURPLE_IS_QUEUED_OUTPUT_STREAM_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS((o), PURPLE_TYPE_QUEUED_OUTPUT_STREAM, PurpleQueuedOutputStreamClass))

/**
 * PurpleQueuedOutputStream:
 *
 * An implementation of #GFilterOutputStream which allows queuing data for
 * output. This allows data to be queued while other data is being output.
 * Therefore, data doesn't have to be manually stored while waiting for
 * stream operations to finish.
 *
 * To create a queued output stream, use #purple_queued_output_stream_new().
 *
 * To queue data, use #purple_queued_output_stream_push_bytes().
 *
 * Once data has been queued, flush the stream with #g_output_stream_flush()
 * or #g_output_stream_flush_async().
 */
typedef struct _PurpleQueuedOutputStream	PurpleQueuedOutputStream;
typedef struct _PurpleQueuedOutputStreamClass	PurpleQueuedOutputStreamClass;
typedef struct _PurpleQueuedOutputStreamPrivate	PurpleQueuedOutputStreamPrivate;

struct _PurpleQueuedOutputStream
{
	GFilterOutputStream parent_instance;

	/*< protected >*/
	PurpleQueuedOutputStreamPrivate *priv;
};

struct _PurpleQueuedOutputStreamClass
{
	GFilterOutputStreamClass parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_g_reserved1)(void);
	void (*_g_reserved2)(void);
};

GType purple_queued_output_stream_get_type(void) G_GNUC_CONST;

/*
 * purple_queued_output_stream_new
 * @base_stream: Base output stream to wrap with the queued stream
 *
 * Creates a new queued output stream for a base stream.
 */
PurpleQueuedOutputStream *purple_queued_output_stream_new(
		GOutputStream *base_stream);

/*
 * purple_queued_output_stream_push_bytes
 * @stream: Stream to push bytes to
 * @bytes: Bytes to queue
 *
 * Queues data to be output through the stream. Flush the stream to
 * output this data.
 */
void purple_queued_output_stream_push_bytes(PurpleQueuedOutputStream *stream,
		GBytes *bytes);

G_END_DECLS

#endif /* _PURPLE_QUEUED_OUTPUT_STREAM_H */
