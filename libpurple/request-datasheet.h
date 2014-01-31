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

#include <glib.h>

typedef struct _PurpleRequestDatasheet PurpleRequestDatasheet;
typedef struct _PurpleRequestDatasheetRecord PurpleRequestDatasheetRecord;
typedef struct _PurpleRequestDatasheetAction PurpleRequestDatasheetAction;

typedef void (*PurpleRequestDatasheetActionCb)(
	PurpleRequestDatasheetRecord *rec, gpointer user_data);
typedef gboolean (*PurpleRequestDatasheetActionCheckCb)(
	PurpleRequestDatasheetRecord *rec, gpointer user_data);

typedef enum
{
	PURPLE_REQUEST_DATASHEET_COLUMN_STRING,
	PURPLE_REQUEST_DATASHEET_COLUMN_IMAGE
} PurpleRequestDatasheetColumnType;

G_BEGIN_DECLS

/**************************************************************************/
/** @name Datasheet API                                                   */
/**************************************************************************/
/*@{*/

/**
 * Creates new Datasheet.
 *
 * Returns: The new datasheet.
 */
PurpleRequestDatasheet *
purple_request_datasheet_new(void);

/**
 * Destroys datasheet with all its contents.
 *
 * @sheet: The datasheet.
 */
void
purple_request_datasheet_free(PurpleRequestDatasheet *sheet);

/**
 * Adds a column to the datasheet.
 *
 * You cannot add a column if datasheet contains any data.
 *
 * @sheet: The datasheet.
 * @type:  The column type.
 * @title: The column title (may be %NULL).
 */
void
purple_request_datasheet_add_column(PurpleRequestDatasheet *sheet,
	PurpleRequestDatasheetColumnType type, const gchar *title);

/**
 * Returns the column count of datasheet.
 *
 * @sheet: The datasheet.
 *
 * Returns: The column count.
 */
guint
purple_request_datasheet_get_column_count(PurpleRequestDatasheet *sheet);

/**
 * Returns the column type for a datasheet.
 *
 * @sheet:  The datasheet.
 * @col_no: The column number (0 is the first one).
 *
 * Returns: The column type.
 */
PurpleRequestDatasheetColumnType
purple_request_datasheet_get_column_type(PurpleRequestDatasheet *sheet,
	guint col_no);

/**
 * Returns the column title for a datasheet.
 *
 * @sheet:  The datasheet.
 * @col_no: The column number (0 is the first one).
 *
 * Returns: The column title.
 */
const gchar *
purple_request_datasheet_get_column_title(PurpleRequestDatasheet *sheet,
	guint col_no);

/**
 * Returns the list of records in a datasheet.
 *
 * You shouldn't modify datasheet's data while iterating through it.
 *
 * @sheet: The datasheet.
 *
 * Returns: (TODO const): The list of records.
 */
const GList *
purple_request_datasheet_get_records(PurpleRequestDatasheet *sheet);

/**
 * Adds an action to the datasheet.
 *
 * Action object is owned by the datasheet since this call.
 *
 * @sheet:  The datasheet.
 * @action: The action.
 */
void
purple_request_datasheet_add_action(PurpleRequestDatasheet *sheet,
	PurpleRequestDatasheetAction *action);

/**
 * Returns the list of actions in a datasheet.
 *
 * @sheet: The datasheet.
 *
 * Returns: (TODO const): The list of actions.
 */
const GList *
purple_request_datasheet_get_actions(PurpleRequestDatasheet *sheet);

/*@}*/


/**************************************************************************/
/** @name Datasheet actions API                                           */
/**************************************************************************/
/*@{*/

/**
 * Creates new datasheet action.
 *
 * Returns: The new action.
 */
PurpleRequestDatasheetAction *
purple_request_datasheet_action_new(void);

/**
 * Destroys the datasheet action.
 *
 * @act: The action.
 */
void
purple_request_datasheet_action_free(PurpleRequestDatasheetAction *act);

/**
 * Sets the localized label for the action.
 *
 * @act:   The action.
 * @label: The label.
 */
void
purple_request_datasheet_action_set_label(PurpleRequestDatasheetAction *act,
	const gchar *label);

/**
 * Gets the label of action.
 *
 * @act: The action.
 *
 * Returns: The localized label text.
 */
const gchar*
purple_request_datasheet_action_get_label(PurpleRequestDatasheetAction *act);

/**
 * Sets the callback for the action.
 *
 * @act:       The action.
 * @cb:        The callback function.
 * @user_data: The data to be passed to the callback function.
 */
void
purple_request_datasheet_action_set_cb(PurpleRequestDatasheetAction *act,
	PurpleRequestDatasheetActionCb cb, gpointer user_data);

/**
 * Calls the callback of the action.
 *
 * @act: The action.
 * @rec: The user selected record.
 */
void
purple_request_datasheet_action_call(PurpleRequestDatasheetAction *act,
	PurpleRequestDatasheetRecord *rec);

/**
 * Sets the sensitivity checker for the action.
 *
 * If there is no callback set, default is used: the action is enabled, if any
 * record is active.
 *
 * @act:       The action.
 * @cb:        The callback function, may be %NULL.
 * @user_data: The data to be passed to the callback function.
 */
void
purple_request_datasheet_action_set_sens_cb(
	PurpleRequestDatasheetAction *act,
	PurpleRequestDatasheetActionCheckCb cb, gpointer user_data);

/**
 * Checks, if the action is enabled for the active record.
 *
 * @act: The action.
 * @rec: The record.
 *
 * Returns: %TRUE, if the action is enabled, %FALSE otherwise.
 */
gboolean
purple_request_datasheet_action_is_sensitive(PurpleRequestDatasheetAction *act,
	PurpleRequestDatasheetRecord *rec);

/*@}*/


/**************************************************************************/
/** @name Datasheet record API                                            */
/**************************************************************************/
/*@{*/

/**
 * Returns the key of a record.
 *
 * @rec: The record.
 *
 * Returns: The key.
 */
gpointer
purple_request_datasheet_record_get_key(
	const PurpleRequestDatasheetRecord *rec);

/**
 * Returns the datasheet of a record.
 *
 * @rec: The record.
 *
 * Returns: The datasheet.
 */
PurpleRequestDatasheet *
purple_request_datasheet_record_get_datasheet(
	PurpleRequestDatasheetRecord *rec);

/**
 * Looks up for a record in datasheet.
 *
 * @sheet: The datasheet.
 * @key:   The key.
 *
 * Returns: The record if found, %NULL otherwise.
 */
PurpleRequestDatasheetRecord *
purple_request_datasheet_record_find(PurpleRequestDatasheet *sheet,
	gpointer key);

/**
 * Adds a record to the datasheet.
 *
 * If the specified key already exists in datasheet, old record is returned.
 *
 * @sheet: The datasheet.
 * @key:   The key.
 *
 * Returns: The record.
 */
PurpleRequestDatasheetRecord *
purple_request_datasheet_record_add(PurpleRequestDatasheet *sheet,
	gpointer key);

/**
 * Removes a record from a datasheet.
 *
 * @sheet: The datasheet.
 * @key:   The key.
 */
void
purple_request_datasheet_record_remove(PurpleRequestDatasheet *sheet,
	gpointer key);

/**
 * Removes all records from a datasheet.
 *
 * @sheet: The datasheet.
 */
void
purple_request_datasheet_record_remove_all(PurpleRequestDatasheet *sheet);

/**
 * Marks all records for removal. Record will be unmarked, when touched with
 * purple_request_datasheet_record_add.
 *
 * @sheet: The datasheet.
 *
 * @see purple_request_datasheet_record_add.
 */
void
purple_request_datasheet_record_mark_all_for_rem(PurpleRequestDatasheet *sheet);

/**
 * Removes all marked records.
 *
 * @sheet: The datasheet.
 *
 * @see purple_request_datasheet_record_mark_all_for_rem.
 */
void
purple_request_datasheet_record_remove_marked(PurpleRequestDatasheet *sheet);

/**
 * Sets data for a string column of specified record.
 *
 * @rec:    The record.
 * @col_no: The column.
 * @data:   The data.
 */
void
purple_request_datasheet_record_set_string_data(
	PurpleRequestDatasheetRecord *rec, guint col_no, const gchar *data);

/**
 * Sets data for a image column of specified record.
 *
 * @rec:    The record.
 * @col_no: The column.
 * @data:   The stock identifier of a image.
 */
void
purple_request_datasheet_record_set_image_data(
	PurpleRequestDatasheetRecord *rec, guint col_no, const gchar *stock_id);

/**
 * Returns data for a string column of specified record.
 *
 * @rec:    The record.
 * @col_no: The column.
 *
 * Returns: The data.
 */
const gchar *
purple_request_datasheet_record_get_string_data(
	const PurpleRequestDatasheetRecord *rec, guint col_no);

/**
 * Returns data for an image column of specified record.
 *
 * @rec:    The record.
 * @col_no: The column.
 *
 * Returns: The stock id of an image.
 */
const gchar *
purple_request_datasheet_record_get_image_data(
	const PurpleRequestDatasheetRecord *rec, guint col_no);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_REQUEST_DATA_H_ */
