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

#include "internal.h"
#include "queuedoutputstream.h"

struct _PurpleQueuedOutputStreamPrivate {
	GAsyncQueue *queue;
};

GObjectClass *parent_class = NULL;

#define PURPLE_QUEUED_OUTPUT_STREAM_GET_PRIVATE(obj) \
		(G_TYPE_INSTANCE_GET_PRIVATE((obj), \
		PURPLE_TYPE_QUEUED_OUTPUT_STREAM, \
		PurpleQueuedOutputStreamPrivate))

G_DEFINE_TYPE_WITH_CODE(PurpleQueuedOutputStream, purple_queued_output_stream,
		G_TYPE_FILTER_OUTPUT_STREAM,
		G_ADD_PRIVATE(PurpleQueuedOutputStream))

static void purple_queued_output_stream_dispose(GObject *object);
static gboolean purple_queued_output_stream_flush(GOutputStream *stream,
		GCancellable *cancellable, GError **error);

static void
purple_queued_output_stream_class_init(PurpleQueuedOutputStreamClass *klass)
{
	GObjectClass *object_class;
	GOutputStreamClass *ostream_class;

	parent_class = g_type_class_peek_parent(klass);

	object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = purple_queued_output_stream_dispose;

	ostream_class = G_OUTPUT_STREAM_CLASS(klass);
	ostream_class->flush = purple_queued_output_stream_flush;
}

PurpleQueuedOutputStream *
purple_queued_output_stream_new(GOutputStream *base_stream)
{
	PurpleQueuedOutputStream *stream;

	g_return_val_if_fail(G_IS_OUTPUT_STREAM(base_stream), NULL);

	stream = g_object_new(PURPLE_TYPE_QUEUED_OUTPUT_STREAM,
			"base-stream", base_stream,
			NULL);

	return stream;
}

void
purple_queued_output_stream_push_bytes(PurpleQueuedOutputStream *stream,
		GBytes *bytes)
{
	g_return_if_fail(PURPLE_QUEUED_OUTPUT_STREAM(stream));
	g_return_if_fail(bytes != NULL);

	g_async_queue_push(stream->priv->queue, g_bytes_ref(bytes));
}

static void
purple_queued_output_stream_init(PurpleQueuedOutputStream *stream)
{
	stream->priv = PURPLE_QUEUED_OUTPUT_STREAM_GET_PRIVATE(stream);
	stream->priv->queue =
			g_async_queue_new_full((GDestroyNotify)g_bytes_unref);
}

static void
purple_queued_output_stream_dispose(GObject *object)
{
	PurpleQueuedOutputStream *stream = PURPLE_QUEUED_OUTPUT_STREAM(object);

	/* Chain up first in case the stream is flushed */
	G_OBJECT_CLASS(parent_class)->dispose(object);

	g_clear_pointer(&stream->priv->queue, g_async_queue_unref);
}

static gboolean
purple_queued_output_stream_flush(GOutputStream *stream,
		GCancellable *cancellable, GError **error)
{
	GOutputStream *base_stream;
	GAsyncQueue *queue;
	GBytes *bytes;
	const void *buffer;
	gsize count;
	gsize bytes_written = 0;
	gboolean ret = TRUE;

	base_stream = g_filter_output_stream_get_base_stream(
			G_FILTER_OUTPUT_STREAM(stream));
	queue = PURPLE_QUEUED_OUTPUT_STREAM(stream)->priv->queue;

	do {
		bytes = g_async_queue_try_pop(queue);

		if (bytes == NULL) {
			break;
		}

		buffer = g_bytes_get_data(bytes, &count);

		ret = g_output_stream_write_all(base_stream, buffer, count,
				&bytes_written, cancellable, error);

		if (!ret) {
			GBytes *queue_bytes;

			if (bytes_written > 0) {
				queue_bytes = g_bytes_new_from_bytes(bytes,
						bytes_written,
						count - bytes_written);
			} else {
				queue_bytes = g_bytes_ref(bytes);
			}

			g_async_queue_push_front(queue, queue_bytes);
		}

		g_bytes_unref(bytes);
	} while (ret);

	return ret;
}

