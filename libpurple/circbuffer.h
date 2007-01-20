/*
 * @file circbuffer.h Buffer Utility Functions
 * @ingroup core
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
#ifndef _CIRCBUFFER_H
#define _CIRCBUFFER_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _GaimCircBuffer {

	/** A pointer to the starting address of our chunk of memory. */
	gchar *buffer;

	/** The incremental amount to increase this buffer by when
	 *  the buffer is not big enough to hold incoming data, in bytes. */
	gsize growsize;

	/** The length of this buffer, in bytes. */
	gsize buflen;

	/** The number of bytes of this buffer that contain unread data. */
	gsize bufused;

	/** A pointer to the next byte where new incoming data is
	 *  buffered to. */
	gchar *inptr;

	/** A pointer to the next byte of buffered data that should be
	 *  read by the consumer. */
	gchar *outptr;

} GaimCircBuffer;

/**
 * Creates a new circular buffer.  This will not allocate any memory for the
 * actual buffer until data is appended to it.
 *
 * @param growsize The amount that the buffer should grow the first time data
 *                 is appended and every time more space is needed.  Pass in
 *                 "0" to use the default of 256 bytes.
 *
 * @return The new GaimCircBuffer. This should be freed with
 *         gaim_circ_buffer_destroy when you are done with it
 */
GaimCircBuffer *gaim_circ_buffer_new(gsize growsize);

/**
 * Dispose of the GaimCircBuffer and free any memory used by it (including any
 * memory used by the internal buffer).
 *
 * @param buf The GaimCircBuffer to free
 */
void gaim_circ_buffer_destroy(GaimCircBuffer *buf);

/**
 * Append data to the GaimCircBuffer.  This will grow the internal
 * buffer to fit the added data, if needed.
 *
 * @param buf The GaimCircBuffer to which to append the data
 * @param src pointer to the data to copy into the buffer
 * @param len number of bytes to copy into the buffer
 */
void gaim_circ_buffer_append(GaimCircBuffer *buf, gconstpointer src, gsize len);

/**
 * Determine the maximum number of contiguous bytes that can be read from the
 * GaimCircBuffer.
 * Note: This may not be the total number of bytes that are buffered - a
 * subsequent call after calling gaim_circ_buffer_mark_read() may indicate more
 * data is available to read.
 *
 * @param buf the GaimCircBuffer for which to determine the maximum contiguous
 *            bytes that can be read.
 *
 * @return the number of bytes that can be read from the GaimCircBuffer
 */
gsize gaim_circ_buffer_get_max_read(GaimCircBuffer *buf);

/**
 * Mark the number of bytes that have been read from the buffer.
 *
 * @param buf The GaimCircBuffer to mark bytes read from
 * @param len The number of bytes to mark as read
 *
 * @return TRUE if we successfully marked the bytes as having been read, FALSE
 *         otherwise.
 */
gboolean gaim_circ_buffer_mark_read(GaimCircBuffer *buf, gsize len);

#ifdef __cplusplus
}
#endif

#endif /* _CIRCBUFFER_H */
