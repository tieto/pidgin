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
	GBytes *next;
};

static GObjectClass *parent_class = NULL;

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
static void purple_queued_output_stream_flush_async(GOutputStream *stream,
		int io_priority, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data);
static gboolean purple_queued_output_stream_flush_finish(GOutputStream *stream,
		GAsyncResult *result, GError **error);

static void purple_queued_output_stream_start_flush_async(GTask *task);

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
	ostream_class->flush_async = purple_queued_output_stream_flush_async;
	ostream_class->flush_finish = purple_queued_output_stream_flush_finish;
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
	g_clear_pointer(&stream->priv->next, g_bytes_unref);
}

static gboolean
purple_queued_output_stream_flush(GOutputStream *stream,
		GCancellable *cancellable, GError **error)
{
	PurpleQueuedOutputStreamPrivate *priv;
	GOutputStream *base_stream;
	GBytes *bytes;
	const void *buffer;
	gsize count;
	gsize bytes_written = 0;
	gboolean ret = TRUE;

	priv = PURPLE_QUEUED_OUTPUT_STREAM(stream)->priv;
	base_stream = g_filter_output_stream_get_base_stream(
			G_FILTER_OUTPUT_STREAM(stream));

	do {
		if (priv->next != NULL) {
			bytes = priv->next;
			priv->next = NULL;
		} else {
			bytes = g_async_queue_try_pop(priv->queue);
		}

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

			priv->next = queue_bytes;
		}

		g_bytes_unref(bytes);
	} while (ret);

	return ret;
}

static void
purple_queued_output_stream_flush_async_cb(GObject *source,
		GAsyncResult *res, gpointer user_data)
{
	GTask *task = user_data;
	PurpleQueuedOutputStream *stream;
	gssize written;
	gsize size;
	GBytes *old_bytes;
	GError *error = NULL;

	written = g_output_stream_write_bytes_finish(G_OUTPUT_STREAM(source),
			res, &error);

	if (written < 0) {
		g_task_return_error(task, error);
		return;
	}

	stream = PURPLE_QUEUED_OUTPUT_STREAM(g_task_get_source_object(task));
	size = g_bytes_get_size(stream->priv->next);

	old_bytes = stream->priv->next;
	stream->priv->next = NULL;

	if (size > (gsize)written) {
		stream->priv->next = g_bytes_new_from_bytes(old_bytes,
				written, size - written);
	}

	g_bytes_unref(old_bytes);

	purple_queued_output_stream_start_flush_async(task);
}

static void
purple_queued_output_stream_start_flush_async(GTask *task)
{
	PurpleQueuedOutputStream *stream;
	GOutputStream *base_stream;

	stream = PURPLE_QUEUED_OUTPUT_STREAM(g_task_get_source_object(task));
	base_stream = g_filter_output_stream_get_base_stream(
			G_FILTER_OUTPUT_STREAM(stream));

	if (stream->priv->next == NULL) {
		stream->priv->next =
				g_async_queue_try_pop(stream->priv->queue);

		if (stream->priv->next == NULL) {
			g_task_return_boolean(task, TRUE);
			return;
		}
	}

	g_output_stream_write_bytes_async(base_stream, stream->priv->next,
			g_task_get_priority(task),
			g_task_get_cancellable(task),
			purple_queued_output_stream_flush_async_cb, task);
}

static void
purple_queued_output_stream_flush_async(GOutputStream *stream,
		int io_priority, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task;

	task = g_task_new(stream, cancellable, callback, user_data);
	g_task_set_priority(task, io_priority);

	purple_queued_output_stream_start_flush_async(task);
}

static gboolean
purple_queued_output_stream_flush_finish(GOutputStream *stream,
		GAsyncResult *result, GError **error)
{
	g_return_val_if_fail(g_task_is_valid(result, stream), FALSE);

	return g_task_propagate_boolean(G_TASK(result), error);
}

