/*
 * Gaim
 *
 * Gaim is the legal property of its developers, whose names are too
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#ifndef _GAIM_MIME_H
#define _GAIM_MIME_H

#include <glib.h>
#include <glib/glist.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file mime.h
 * @ingroup core
 *
 * Rudimentary parsing of multi-part MIME messages into more
 * accessible structures.
 */

/**
 * @typedef GaimMimeDocument A MIME document.
 */
typedef struct _GaimMimeDocument GaimMimeDocument;

/**
 * @typedef GaimMimePart A part of a multipart MIME document.
 */
typedef struct _GaimMimePart GaimMimePart;

/**
 * Allocate an empty MIME document.
 */
GaimMimeDocument *gaim_mime_document_new(void);

/**
 * Frees memory used in a MIME document and all of its parts and fields
 *
 * @param doc The MIME document to free.
 */
void gaim_mime_document_free(GaimMimeDocument *doc);

/**
 * Parse a MIME document from a NUL-terminated string.
 *
 * @param buf The NULL-terminated string containing the MIME-encoded data.
 *
 * @returns A MIME document.
 */
GaimMimeDocument *gaim_mime_document_parse(const char *buf);

/**
 * Parse a MIME document from a string
 *
 * @param buf The string containing the MIME-encoded data.
 * @param len Length of buf.
 *
 * @returns   A MIME document.
 */
GaimMimeDocument *gaim_mime_document_parsen(const char *buf, gsize len);

/**
 * Write (append) a MIME document onto a GString.
 */
void gaim_mime_document_write(GaimMimeDocument *doc, GString *str);

/**
 * The list of fields in the header of a document
 *
 * @param doc The MIME document.
 *
 * @returns   A list of strings indicating the fields (but not the values of
 *            the fields) in the header of doc.
 */
const GList *gaim_mime_document_get_fields(GaimMimeDocument *doc);

/**
 * Get the value of a specific field in the header of a document.
 *
 * @param doc   The MIME document.
 * @param field Case-insensitive field name.
 *
 * @returns     Value associated with the indicated header field, or
 *              NULL if the field doesn't exist.
 */
const char *gaim_mime_document_get_field(GaimMimeDocument *doc,
					 const char *field);

/**
 * Set or replace the value of a specific field in the header of a
 * document.
 *
 * @param doc   The MIME document.
 * @param field Case-insensitive field name.
 * @param value Value to associate with the indicated header field,
 *              of NULL to remove the field.
 */
void gaim_mime_document_set_field(GaimMimeDocument *doc,
				  const char *field,
				  const char *value);

/**
 * The list of parts in a multipart document.
 *
 * @param doc The MIME document.
 *
 * @returns   List of GaimMimePart contained within doc.
 */
const GList *gaim_mime_document_get_parts(GaimMimeDocument *doc);

/**
 * Create and insert a new part into a MIME document.
 *
 * @param doc The new part's parent MIME document.
 */
GaimMimePart *gaim_mime_part_new(GaimMimeDocument *doc);


/**
 * The list of fields in the header of a document part.
 *
 * @param part The MIME document part.
 *
 * @returns    List of strings indicating the fields (but not the values
 *             of the fields) in the header of part.
 */
const GList *gaim_mime_part_get_fields(GaimMimePart *part);


/**
 * Get the value of a specific field in the header of a document part.
 *
 * @param part  The MIME document part.
 * @param field Case-insensitive name of the header field.
 *
 * @returns     Value of the specified header field, or NULL if the
 *              field doesn't exist.
 */
const char *gaim_mime_part_get_field(GaimMimePart *part,
				     const char *field);

/**
 * Get the decoded value of a specific field in the header of a
 * document part.
 */
char *gaim_mime_part_get_field_decoded(GaimMimePart *part,
				       const char *field);

/**
 * Set or replace the value of a specific field in the header of a
 * document.
 *
 * @param part  The part of the MIME document.
 * @param field Case-insensitive field name
 * @param value Value to associate with the indicated header field,
 *              of NULL to remove the field.
 */
void gaim_mime_part_set_field(GaimMimePart *part,
			      const char *field,
			      const char *value);

/**
 * Get the (possibly encoded) data portion of a MIME document part.
 *
 * @param part The MIME document part.
 *
 * @returns    NULL-terminated data found in the document part
 */
const char *gaim_mime_part_get_data(GaimMimePart *part);

/**
 * Get the data portion of a MIME document part, after attempting to
 * decode it according to the content-transfer-encoding field. If the
 * specified encoding method is not supported, this function will
 * return NULL.
 *
 * @param part The MIME documemt part.
 * @param data Buffer for the data.
 * @param len  The length of the buffer.
 */
void gaim_mime_part_get_data_decoded(GaimMimePart *part,
				     guchar **data, gsize *len);

/**
 * Get the length of the data portion of a MIME document part.
 *
 * @param part The MIME document part.
 * @returns    Length of the data in the document part.
 */
gsize gaim_mime_part_get_length(GaimMimePart *part);

void gaim_mime_part_set_data(GaimMimePart *part, const char *data);

#ifdef __cplusplus
}
#endif

#endif
