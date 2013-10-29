/**
 * @file request-datasheet.h Request Datasheet API
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */
#ifndef _PURPLE_REQUEST_DATA_H_
#define _PURPLE_REQUEST_DATA_H_

typedef struct _PurpleRequestDatasheet PurpleRequestDatasheet;
typedef struct _PurpleRequestDatasheetRecord PurpleRequestDatasheetRecord;

typedef enum
{
	PURPLE_REQUEST_DATASHEET_COLUMN_STRING,
	PURPLE_REQUEST_DATASHEET_COLUMN_IMAGE
} PurpleRequestDatasheetColumnType;

#include <glib.h>

G_BEGIN_DECLS

/**************************************************************************/
/** @name Datasheet API                                                   */
/**************************************************************************/
/*@{*/

/**
 * Creates new Datasheet.
 *
 * @return The new datasheet.
 */
PurpleRequestDatasheet *
purple_request_datasheet_new(void);

/**
 * Destroys datasheet with all its contents.
 *
 * @param sheet The datasheet.
 */
void
purple_request_datasheet_free(PurpleRequestDatasheet *sheet);

/**
 * Adds a column to the datasheet.
 *
 * You cannot add a column if datasheet contains any data.
 *
 * @param sheet The datasheet.
 * @param type  The column type.
 * @param title The column title (may be @c NULL).
 */
void
purple_request_datasheet_add_column(PurpleRequestDatasheet *sheet,
	PurpleRequestDatasheetColumnType type, const gchar *title);

/**
 * Returns the column count of datasheet.
 *
 * @param sheet The datasheet.
 *
 * @return The column count.
 */
guint
purple_request_datasheet_get_column_count(PurpleRequestDatasheet *sheet);

/**
 * Returns the column type for a datasheet.
 *
 * @param sheet  The datasheet.
 * @param col_no The column number (0 is the first one).
 *
 * @return The column type.
 */
PurpleRequestDatasheetColumnType
purple_request_datasheet_get_column_type(PurpleRequestDatasheet *sheet,
	guint col_no);

/**
 * Returns the column title for a datasheet.
 *
 * @param sheet  The datasheet.
 * @param col_no The column number (0 is the first one).
 *
 * @return The column title.
 */
const gchar *
purple_request_datasheet_get_column_title(PurpleRequestDatasheet *sheet,
	guint col_no);

/**
 * Returns the list of records in a datasheet.
 *
 * You shouldn't modify datasheet's data while iterating through it.
 *
 * @param sheet The datasheet.
 *
 * @constreturn The list of records.
 */
const GList *
purple_request_datasheet_get_records(PurpleRequestDatasheet *sheet);

/*@}*/


/**************************************************************************/
/** @name Datasheet record API                                            */
/**************************************************************************/
/*@{*/

/**
 * Returns the key of a record.
 *
 * @param rec The record.
 *
 * @return The key.
 */
gpointer
purple_request_datasheet_record_get_key(
	const PurpleRequestDatasheetRecord *rec);

/**
 * Returns the datasheet of a record.
 *
 * @param rec The record.
 *
 * @return The datasheet.
 */
PurpleRequestDatasheet *
purple_request_datasheet_record_get_datasheet(
	PurpleRequestDatasheetRecord *rec);

/**
 * Looks up for a record in datasheet.
 *
 * @param sheet The datasheet.
 * @param key   The key.
 *
 * @return The record if found, @c NULL otherwise.
 */
PurpleRequestDatasheetRecord *
purple_request_datasheet_record_find(PurpleRequestDatasheet *sheet,
	gpointer key);

/**
 * Adds a record to the datasheet.
 *
 * If the specified key already exists in datasheet, old record is returned.
 *
 * @param sheet The datasheet.
 * @param key   The key.
 *
 * @return The record.
 */
PurpleRequestDatasheetRecord *
purple_request_datasheet_record_add(PurpleRequestDatasheet *sheet,
	gpointer key);

/**
 * Removes a record from a datasheet.
 *
 * @param sheet The datasheet.
 * @param key   The key.
 */
void
purple_request_datasheet_record_remove(PurpleRequestDatasheet *sheet,
	gpointer key);

/**
 * Removes all records from a datasheet.
 *
 * @param sheet The datasheet.
 */
void
purple_request_datasheet_record_remove_all(PurpleRequestDatasheet *sheet);

/**
 * Sets data for a string column of specified record.
 *
 * @param rec    The record.
 * @param col_no The column.
 * @param data   The data.
 */
void
purple_request_datasheet_record_set_string_data(
	PurpleRequestDatasheetRecord *rec, guint col_no, const gchar *data);

/**
 * Sets data for a image column of specified record.
 *
 * @param rec    The record.
 * @param col_no The column.
 * @param data   The stock identifier of a image.
 */
void
purple_request_datasheet_record_set_image_data(
	PurpleRequestDatasheetRecord *rec, guint col_no, const gchar *stock_id);

/**
 * Returns data for a string column of specified record.
 *
 * @param rec    The record.
 * @param col_no The column.
 *
 * @return The data.
 */
const gchar *
purple_request_datasheet_record_get_string_data(
	const PurpleRequestDatasheetRecord *rec, guint col_no);

/**
 * Returns data for an image column of specified record.
 *
 * @param rec    The record.
 * @param col_no The column.
 *
 * @return The stock id of an image.
 */
const gchar *
purple_request_datasheet_record_get_image_data(
	const PurpleRequestDatasheetRecord *rec, guint col_no);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_REQUEST_DATA_H_ */
