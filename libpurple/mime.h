/*
 * Purple
 *
 * Purple is the legal property of its developers, whose names are too
 * numerous to list here. Please refer to the COPYRIGHT file distributed
 * with this source distribution
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301,
 * USA.
 */

#ifndef _PURPLE_MIME_H
#define _PURPLE_MIME_H

#include <glib.h>

/**
 * @file mime.h
 * @ingroup core
 *
 * Rudimentary parsing of multi-part MIME messages into more
 * accessible structures.
 */

/**
 * A MIME document.
 */
typedef struct _PurpleMimeDocument PurpleMimeDocument;

/**
 * A part of a multipart MIME document.
 */
typedef struct _PurpleMimePart PurpleMimePart;

G_BEGIN_DECLS

/**
 * Allocate an empty MIME document.
 */
PurpleMimeDocument *purple_mime_document_new(void);

/**
 * Frees memory used in a MIME document and all of its parts and fields
 *
 * @doc: The MIME document to free.
 */
void purple_mime_document_free(PurpleMimeDocument *doc);

/**
 * Parse a MIME document from a NUL-terminated string.
 *
 * @buf: The NULL-terminated string containing the MIME-encoded data.
 *
 * Returns:s A MIME document.
 */
PurpleMimeDocument *purple_mime_document_parse(const char *buf);

/**
 * Parse a MIME document from a string
 *
 * @buf: The string containing the MIME-encoded data.
 * @len: Length of buf.
 *
 * Returns:s   A MIME document.
 */
PurpleMimeDocument *purple_mime_document_parsen(const char *buf, gsize len);

/**
 * Write (append) a MIME document onto a GString.
 */
void purple_mime_document_write(PurpleMimeDocument *doc, GString *str);

/**
 * The list of fields in the header of a document
 *
 * @doc: The MIME document.
 *
 * Returns: (TODO const): A list of strings indicating the fields (but not the values
 *              of the fields) in the header of doc.
 */
GList *purple_mime_document_get_fields(PurpleMimeDocument *doc);

/**
 * Get the value of a specific field in the header of a document.
 *
 * @doc:   The MIME document.
 * @field: Case-insensitive field name.
 *
 * Returns:s     Value associated with the indicated header field, or
 *              NULL if the field doesn't exist.
 */
const char *purple_mime_document_get_field(PurpleMimeDocument *doc,
					 const char *field);

/**
 * Set or replace the value of a specific field in the header of a
 * document.
 *
 * @doc:   The MIME document.
 * @field: Case-insensitive field name.
 * @value: Value to associate with the indicated header field,
 *              of NULL to remove the field.
 */
void purple_mime_document_set_field(PurpleMimeDocument *doc,
				  const char *field,
				  const char *value);

/**
 * The list of parts in a multipart document.
 *
 * @doc: The MIME document.
 *
 * Returns: (TODO const):   List of PurpleMimePart contained within doc.
 */
GList *purple_mime_document_get_parts(PurpleMimeDocument *doc);

/**
 * Create and insert a new part into a MIME document.
 *
 * @doc: The new part's parent MIME document.
 */
PurpleMimePart *purple_mime_part_new(PurpleMimeDocument *doc);


/**
 * The list of fields in the header of a document part.
 *
 * @part: The MIME document part.
 *
 * Returns: (TODO const): List of strings indicating the fields (but not the values
 *              of the fields) in the header of part.
 */
GList *purple_mime_part_get_fields(PurpleMimePart *part);


/**
 * Get the value of a specific field in the header of a document part.
 *
 * @part:  The MIME document part.
 * @field: Case-insensitive name of the header field.
 *
 * Returns:s     Value of the specified header field, or NULL if the
 *              field doesn't exist.
 */
const char *purple_mime_part_get_field(PurpleMimePart *part,
				     const char *field);

/**
 * Get the decoded value of a specific field in the header of a
 * document part.
 */
char *purple_mime_part_get_field_decoded(PurpleMimePart *part,
				       const char *field);

/**
 * Set or replace the value of a specific field in the header of a
 * document.
 *
 * @part:  The part of the MIME document.
 * @field: Case-insensitive field name
 * @value: Value to associate with the indicated header field,
 *              of NULL to remove the field.
 */
void purple_mime_part_set_field(PurpleMimePart *part,
			      const char *field,
			      const char *value);

/**
 * Get the (possibly encoded) data portion of a MIME document part.
 *
 * @part: The MIME document part.
 *
 * Returns:s    NULL-terminated data found in the document part
 */
const char *purple_mime_part_get_data(PurpleMimePart *part);

/**
 * Get the data portion of a MIME document part, after attempting to
 * decode it according to the content-transfer-encoding field. If the
 * specified encoding method is not supported, this function will
 * return NULL.
 *
 * @part: The MIME documemt part.
 * @data: Buffer for the data.
 * @len:  The length of the buffer.
 */
void purple_mime_part_get_data_decoded(PurpleMimePart *part,
				     guchar **data, gsize *len);

/**
 * Get the length of the data portion of a MIME document part.
 *
 * @part: The MIME document part.
 * Returns:s    Length of the data in the document part.
 */
gsize purple_mime_part_get_length(PurpleMimePart *part);

void purple_mime_part_set_data(PurpleMimePart *part, const char *data);

G_END_DECLS

#endif
