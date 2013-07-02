/**
 * @file circbuffer.h Buffer Utility Functions
 * @ingroup core
 */

/* Purple is the legal property of its developers, whose names are too numerous
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
#ifndef PURPLE_CIRCULAR_BUFFER_H
#define PURPLE_CIRCULAR_BUFFER_H

#include <glib.h>
#include <glib-object.h>

#define PURPLE_TYPE_CIRCULAR_BUFFER            (purple_circular_buffer_get_type())
#define PURPLE_CIRCULAR_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CIRCULAR_BUFFER, PurpleCircularBuffer))
#define PURPLE_CIRCULAR_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CIRCULAR_BUFFER, PurpleCircularBufferClass))
#define PURPLE_IS_CIRCULAR_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CIRCULAR_BUFFER))
#define PURPLE_IS_CIRCULAR_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CIRCULAR_BUFFER))
#define PURPLE_CIRCULAR_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CIRCULAR_BUFFER, PurpleCircularBufferClass))

typedef struct _PurpleCircularBuffer           PurpleCircularBuffer;
typedef struct _PurpleCircularBufferClass      PurpleCircularBufferClass;

struct _PurpleCircularBuffer {
	/*< private >*/
	GObject parent;
};

struct _PurpleCircularBufferClass {
	/*< private >*/
	GObjectClass parent;

	void (*grow)(PurpleCircularBuffer *buffer, gsize len);
	void (*append)(PurpleCircularBuffer *buffer, gconstpointer src, gsize len);
	gsize (*max_read_size)(const PurpleCircularBuffer *buffer);
	gboolean (*mark_read)(PurpleCircularBuffer *buffer, gsize len);

	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

G_BEGIN_DECLS

GType purple_circular_buffer_get_type(void);

/**
 * Creates a new circular buffer.  This will not allocate any memory for the
 * actual buffer until data is appended to it.
 *
 * @param growsize The amount that the buffer should grow the first time data
 *                 is appended and every time more space is needed.  Pass in
 *                 "0" to use the default of 256 bytes.
 *
 * @return The new PurpleCircBuffer. This should be freed with
 *         purple_circ_buffer_destroy when you are done with it
 */
PurpleCircularBuffer *purple_circular_buffer_new(gsize growsize);

/**
 * Append data to the PurpleCircBuffer.  This will grow the internal
 * buffer to fit the added data, if needed.
 *
 * @param buf The PurpleCircBuffer to which to append the data
 * @param src pointer to the data to copy into the buffer
 * @param len number of bytes to copy into the buffer
 */
void purple_circular_buffer_append(PurpleCircularBuffer *buf, gconstpointer src, gsize len);

/**
 * Determine the maximum number of contiguous bytes that can be read from the
 * PurpleCircBuffer.
 * Note: This may not be the total number of bytes that are buffered - a
 * subsequent call after calling purple_circ_buffer_mark_read() may indicate more
 * data is available to read.
 *
 * @param buf the PurpleCircBuffer for which to determine the maximum contiguous
 *            bytes that can be read.
 *
 * @return the number of bytes that can be read from the PurpleCircBuffer
 */
gsize purple_circular_buffer_get_max_read(const PurpleCircularBuffer *buf);

/**
 * Mark the number of bytes that have been read from the buffer.
 *
 * @param buf The PurpleCircBuffer to mark bytes read from
 * @param len The number of bytes to mark as read
 *
 * @return TRUE if we successfully marked the bytes as having been read, FALSE
 *         otherwise.
 */
gboolean purple_circular_buffer_mark_read(PurpleCircularBuffer *buf, gsize len);

void purple_circular_buffer_grow(PurpleCircularBuffer *buffer, gsize len);
gsize purple_circular_buffer_get_grow_size(const PurpleCircularBuffer *buffer);
gsize purple_circular_buffer_get_used(const PurpleCircularBuffer *buffer);
const gchar *purple_circular_buffer_get_input(const PurpleCircularBuffer *buffer);
const gchar *purple_circular_buffer_get_output(const PurpleCircularBuffer *buffer);
void purple_circular_buffer_reset(PurpleCircularBuffer *buffer);

G_END_DECLS

#endif /* PURPLE_CIRCULAR_BUFFER_H */

