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
/**
 * SECTION:mime
 * @section_id: libpurple-mime
 * @short_description: <filename>mime.h</filename>
 * @title: Multi-part MIME message parsing
 *
 * Rudimentary parsing of multi-part MIME messages into more
 * accessible structures.
 */

#ifndef _PURPLE_MIME_H
#define _PURPLE_MIME_H

#include <glib.h>

/**
 * PurpleMimeDocument:
 *
 * A MIME document.
 */
typedef struct _PurpleMimeDocument PurpleMimeDocument;

/**
 * PurpleMimePart:
 *
 * A part of a multipart MIME document.
 */
typedef struct _PurpleMimePart PurpleMimePart;

G_BEGIN_DECLS

/**
 * purple_mime_document_new:
 *
 * Allocate an empty MIME document.
 */
PurpleMimeDocument *purple_mime_document_new(void);

/**
 * purple_mime_document_free:
 * @doc: The MIME document to free.
 *
 * Frees memory used in a MIME document and all of its parts and fields
 */
void purple_mime_document_free(PurpleMimeDocument *doc);

/**
 * purple_mime_document_parse:
 * @buf: The NULL-terminated string containing the MIME-encoded data.
 *
 * Parse a MIME document from a NUL-terminated string.
 *
 * Returns: A MIME document.
 */
PurpleMimeDocument *purple_mime_document_parse(const char *buf);

/**
 * purple_mime_document_parsen:
 * @buf: The string containing the MIME-encoded data.
 * @len: Length of buf.
 *
 * Parse a MIME document from a string
 *
 * Returns:   A MIME document.
 */
PurpleMimeDocument *purple_mime_document_parsen(const char *buf, gsize len);

/**
 * purple_mime_document_write:
 *
 * Write (append) a MIME document onto a GString.
 */
void purple_mime_document_write(PurpleMimeDocument *doc, GString *str);

/**
 * purple_mime_document_get_fields:
 * @doc: The MIME document.
 *
 * The list of fields in the header of a document
 *
 * Returns: (transfer none): A list of strings indicating the fields (but not
 *          the values of the fields) in the header of doc.
 */
GList *purple_mime_document_get_fields(PurpleMimeDocument *doc);

/**
 * purple_mime_document_get_field:
 * @doc:   The MIME document.
 * @field: Case-insensitive field name.
 *
 * Get the value of a specific field in the header of a document.
 *
 * Returns:     Value associated with the indicated header field, or
 *              NULL if the field doesn't exist.
 */
const char *purple_mime_document_get_field(PurpleMimeDocument *doc,
					 const char *field);

/**
 * purple_mime_document_set_field:
 * @doc:   The MIME document.
 * @field: Case-insensitive field name.
 * @value: Value to associate with the indicated header field,
 *              of NULL to remove the field.
 *
 * Set or replace the value of a specific field in the header of a
 * document.
 */
void purple_mime_document_set_field(PurpleMimeDocument *doc,
				  const char *field,
				  const char *value);

/**
 * purple_mime_document_get_parts:
 * @doc: The MIME document.
 *
 * The list of parts in a multipart document.
 *
 * Returns: (transfer none):   List of PurpleMimePart contained within doc.
 */
GList *purple_mime_document_get_parts(PurpleMimeDocument *doc);

/**
 * purple_mime_part_new:
 * @doc: The new part's parent MIME document.
 *
 * Create and insert a new part into a MIME document.
 */
PurpleMimePart *purple_mime_part_new(PurpleMimeDocument *doc);


/**
 * purple_mime_part_get_fields:
 * @part: The MIME document part.
 *
 * The list of fields in the header of a document part.
 *
 * Returns: (transfer none): List of strings indicating the fields (but not the
 *          values of the fields) in the header of part.
 */
GList *purple_mime_part_get_fields(PurpleMimePart *part);


/**
 * purple_mime_part_get_field:
 * @part:  The MIME document part.
 * @field: Case-insensitive name of the header field.
 *
 * Get the value of a specific field in the header of a document part.
 *
 * Returns:     Value of the specified header field, or NULL if the
 *              field doesn't exist.
 */
const char *purple_mime_part_get_field(PurpleMimePart *part,
				     const char *field);

/**
 * purple_mime_part_get_field_decoded:
 *
 * Get the decoded value of a specific field in the header of a
 * document part.
 */
char *purple_mime_part_get_field_decoded(PurpleMimePart *part,
				       const char *field);

/**
 * purple_mime_part_set_field:
 * @part:  The part of the MIME document.
 * @field: Case-insensitive field name
 * @value: Value to associate with the indicated header field,
 *              of NULL to remove the field.
 *
 * Set or replace the value of a specific field in the header of a
 * document.
 */
void purple_mime_part_set_field(PurpleMimePart *part,
			      const char *field,
			      const char *value);

/**
 * purple_mime_part_get_data:
 * @part: The MIME document part.
 *
 * Get the (possibly encoded) data portion of a MIME document part.
 *
 * Returns:    NULL-terminated data found in the document part
 */
const char *purple_mime_part_get_data(PurpleMimePart *part);

/**
 * purple_mime_part_get_data_decoded:
 * @part: The MIME documemt part.
 * @data: Buffer for the data.
 * @len:  The length of the buffer.
 *
 * Get the data portion of a MIME document part, after attempting to
 * decode it according to the content-transfer-encoding field. If the
 * specified encoding method is not supported, this function will
 * return NULL.
 */
void purple_mime_part_get_data_decoded(PurpleMimePart *part,
				     guchar **data, gsize *len);

/**
 * purple_mime_part_get_length:
 * @part: The MIME document part.
 *
 * Get the length of the data portion of a MIME document part.
 *
 * Returns:    Length of the data in the document part.
 */
gsize purple_mime_part_get_length(PurpleMimePart *part);

void purple_mime_part_set_data(PurpleMimePart *part, const char *data);

G_END_DECLS

#endif
